/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) <2015>, Sylvain Boily
 *
 * Sylvain Boily <sboily@avencall.com>
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 *
 * Please follow coding guidelines
 * https://wiki.asterisk.org/wiki/display/AST/Coding+Guidelines
 */

/*! \file
 *
 * \brief Consul discovery module ressource
 *
 * \author\verbatim Sylvain Boily <sboily@avencall.com> \endverbatim
 *
 * This is a resource to discovery an Asterisk application via Consul
 * \ingroup applications
 */

/*! \li \ref res_discovery_consul.c uses configuration file \ref res_discovery_consul.conf
 * \addtogroup configuration_file Configuration Files
 */

/*! 
 * \page res_discovery_consul.conf res_discovery_consul.conf
 * \verbinclude res_discovery_consul.conf.sample
 */

/*** MODULEINFO
	<defaultenabled>no</defaultenabled>
	<support_level>extends</support_level>
 ***/

/*! \requirements
 *
 * libcurl - http://curl.haxx.se/libcurl/c
 * asterisk - http://asterisk.org
 *
 * Build:
 *
 * make
 * make install
 * make samples
 * 
 */

#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <asterisk.h>
#include <asterisk/module.h>
#include <asterisk/config.h>
#include <asterisk/json.h>

struct curl_put_data {
	char *data;
	size_t size;
};

typedef struct discovery_config {
	int enabled;
        int debug;
        char id[256];
        char name[256];
        char host[256];
        char address_ip[256];
        int port;
        char register_url[256];
        char deregister_url[256];
        char tags[256];
} discovery_config;

static struct discovery_config global_config = {
	.enabled = 1,
	.debug = 0,
	.id = "asterisk",
        .name = "Asterisk",
	.host = "127.0.0.1",
	.address_ip = "127.0.0.1",
	.port = 8500,
	.register_url = "/v1/agent/service/register",
	.deregister_url = "/v1/agent/service/deregister",
	.tags = "asterisk"
};

static const char config_file[] = "res_discovery_consul.conf";

size_t readData(char *ptr, size_t size, size_t nmemb, void* data);
CURLcode consul_deregister(CURL *curl);
CURLcode consul_register(CURL *curl);

size_t readData(char *ptr, size_t size, size_t nmemb, void* data)
{
	size_t realsize = size * nmemb;
	if(realsize < 1)
		return 0;

	struct curl_put_data* mem = (struct curl_put_data*) data;
	if (mem->size > 0) {
		size_t bytes_to_copy = (mem->size > realsize) ? realsize : mem->size;
		memcpy(ptr,mem->data,bytes_to_copy);
		mem->data += bytes_to_copy;
		mem->size -= bytes_to_copy;
		return bytes_to_copy;
	}

	return 0;
}

static struct ast_json *consul_put_json(void) {
	RAII_VAR(struct ast_json *, obj, ast_json_object_create(), ast_json_unref);

	if (!obj) {
		return NULL;
	}

	ast_json_object_set(obj, "ID", ast_json_string_create(global_config.id));
	ast_json_object_set(obj, "Name", ast_json_string_create(global_config.name));
	ast_json_object_set(obj, "Address", ast_json_string_create(global_config.address_ip));
	ast_json_object_set(obj, "Port", ast_json_integer_create(5060));

	return ast_json_ref(obj);
}

CURLcode consul_deregister(CURL *curl)
{
	CURLcode rcode;
	struct curl_slist *headers = NULL;
	char *url = (char *) malloc(1024);

        sprintf(url, "http://%s:%d%s/%s", global_config.host, global_config.port, global_config.deregister_url, global_config.id);
	ast_log(LOG_NOTICE, "Deregister URL: %s\n", url);

	headers = curl_slist_append(headers, "Accept: application/json");
	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, "charsets: utf-8");

	curl_easy_setopt(curl, CURLOPT_VERBOSE, global_config.debug);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1);

	rcode = curl_easy_perform(curl);
	curl_slist_free_all(headers);
        free(url);

	return rcode;
}

CURLcode consul_register(CURL *curl)
{
	CURLcode rcode;
	struct curl_put_data putData = {0,0};
	struct curl_slist *headers = NULL;
	char *url = (char *) malloc(1024);
	struct ast_json *obj;

        sprintf(url, "http://%s:%d%s", global_config.host, global_config.port, global_config.register_url);
	ast_log(LOG_NOTICE, "Register URL: %s\n", url);

	headers = curl_slist_append(headers, "Accept: application/json");
	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, "charsets: utf-8");

	obj = consul_put_json();

	ast_log(LOG_NOTICE, "The json object created: %s : %zu\n", ast_json_dump_string_format(obj, AST_JSON_COMPACT),
                                                                   strlen(ast_json_dump_string_format(obj, AST_JSON_COMPACT)));

	putData.data = (char *) malloc(strlen(ast_json_dump_string_format(obj, AST_JSON_COMPACT)));
	memcpy(putData.data, ast_json_dump_string_format(obj, AST_JSON_COMPACT),
                                                         strlen(ast_json_dump_string_format(obj, AST_JSON_COMPACT)));
	putData.size = strlen(ast_json_dump_string_format(obj, AST_JSON_COMPACT));


	curl_easy_setopt(curl, CURLOPT_VERBOSE, global_config.debug);
	curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)putData.size);

	curl_easy_setopt(curl, CURLOPT_READDATA, (void *) &putData);
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, readData);

	rcode = curl_easy_perform(curl);

        ast_json_free(obj);
	curl_slist_free_all(headers);
        free(url);

	return rcode;
}

/*! \brief Function called to load or reload the configuration file */
static void load_config(int reload)
{
	struct ast_config *cfg = NULL;

	struct ast_flags config_flags = { reload ? CONFIG_FLAG_FILEUNCHANGED : 0 };
	struct ast_variable *v;

	if (!(cfg = ast_config_load(config_file, config_flags)) || cfg == CONFIG_STATUS_FILEINVALID) {
		ast_log(LOG_ERROR, "res_discovery_consul configuration file '%s' not found\n", config_file);
		return;
	} else if (cfg == CONFIG_STATUS_FILEUNCHANGED) {
		return;
	}

	for (v = ast_variable_browse(cfg, "general"); v; v = v->next) {
		if (!strcasecmp(v->name, "enabled")) {
                        ast_log(LOG_NOTICE, "enabled : %s\n", v->value);
			global_config.enabled = atoi(v->value);
		} else if (!strcasecmp(v->name, "debug")) {
                        ast_log(LOG_NOTICE, "debug : %s\n", v->value);
			global_config.debug = atoi(v->value);
                }
        }

	for (v = ast_variable_browse(cfg, "consul"); v; v = v->next) {
		if (!strcasecmp(v->name, "id")) {
			ast_copy_string(global_config.id, v->value, strlen(v->value) + 1);
                        ast_log(LOG_NOTICE, "id : %s\n", v->value);
		} else if (!strcasecmp(v->name, "host")) {
                        ast_log(LOG_NOTICE, "host : %s\n", v->value);
			ast_copy_string(global_config.host, v->value, strlen(v->value) + 1);
		} else if (!strcasecmp(v->name, "port")) {
                        ast_log(LOG_NOTICE, "port : %s\n", v->value);
			global_config.port = atoi(v->value);
		} else if (!strcasecmp(v->name, "register_url")) {
                        ast_log(LOG_NOTICE, "register_url : %s\n", v->value);
			ast_copy_string(global_config.register_url, v->value, strlen(v->value) + 1);
		} else if (!strcasecmp(v->name, "deregister_url")) {
                        ast_log(LOG_NOTICE, "deregister_url : %s\n", v->value);
			ast_copy_string(global_config.deregister_url, v->value, strlen(v->value) + 1);
		} else if (!strcasecmp(v->name, "tags")) {
                        ast_log(LOG_NOTICE, "tags : %s\n", v->value);
			ast_copy_string(global_config.tags, v->value, strlen(v->value) + 1);
		} else if (!strcasecmp(v->name, "name")) {
                        ast_log(LOG_NOTICE, "name : %s\n", v->value);
			ast_copy_string(global_config.name, v->value, strlen(v->value) + 1);
		} else if (!strcasecmp(v->name, "address_ip")) {
                        ast_log(LOG_NOTICE, "address_ip : %s\n", v->value);
			ast_copy_string(global_config.address_ip, v->value, strlen(v->value) + 1);
                }
	}

	ast_config_destroy(cfg);

	return;
}

static void load_res(int start)
{
	CURL *curl;
	CURLcode rcode;

	curl = curl_easy_init();
	load_config(0);

        if (start == 1) {
        	rcode = consul_register(curl);
        } else {
        	rcode = consul_deregister(curl);
        }

	if(rcode != CURLE_OK)
                ast_log(LOG_NOTICE, "curl_easy_perform() failed: %s\n", curl_easy_strerror(rcode));
 
	curl_easy_cleanup(curl);
}


/*! \brief Function called to reload the module */
static int reload_module(void)
{
	load_config(1);
	return 0;
}

/*! \brief Function called to unload the module */
static int unload_module(void)
{
	load_res(0);
	return 0;
}

/*!
 * \brief Load the module
 *
 * Module loading including tests for configuration or dependencies.
 * This function can return AST_MODULE_LOAD_FAILURE, AST_MODULE_LOAD_DECLINE,
 * or AST_MODULE_LOAD_SUCCESS. If a dependency or environment variable fails
 * tests return AST_MODULE_LOAD_FAILURE. If the module can not load the 
 * configuration file or other non-critical problem return 
 * AST_MODULE_LOAD_DECLINE. On success return AST_MODULE_LOAD_SUCCESS.
 */
static int load_module(void)
{
	load_res(1);
	return AST_MODULE_LOAD_SUCCESS;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "Asterisk Discovery CONSUL",
	.support_level = AST_MODULE_SUPPORT_CORE,
	.load = load_module,
	.unload = unload_module,
	.reload = reload_module,
);
