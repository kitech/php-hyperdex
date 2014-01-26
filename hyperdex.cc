/*
  +----------------------------------------------------------------------+
  | HyperDex client PHP plugin                                           |
  +----------------------------------------------------------------------+
  | Copyright (c) 2012 Triton Digital Media                              |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Brad Broerman ( bbroerman@tritonmedia.com )                  |
  +----------------------------------------------------------------------+
 */

/* $Id: hyperdex.cc,v 1.5 2013/04/22 10:33:55 shreos Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_hyperdex.h"


ZEND_DECLARE_MODULE_GLOBALS(hyperdex)

/*
 * Class entries for the hyperdex client class and the exception
 */
extern zend_class_entry* hyperdex_client_cmdex_ce;
extern zend_class_entry* hyperdex_client_ce_exception;

extern zend_object_handlers hyperdex_client_object_handlers;

/*
 * Class entries for the hyperdex admin class and the exception
 */
extern zend_class_entry* hyperdex_admin_cmdex_ce;
extern zend_class_entry* hyperdex_admin_ce_exception;
 
extern zend_object_handlers hyperdex_admin_object_handlers;


/* {{{ hyperdex_client_functions[]
 *
 * Every user visible function must have an entry in hyperdex_client_functions[].
 */
extern zend_function_entry hyperdex_client_functions[];
/* }}} */


/* {{{ hyperdex_admin_functions[]
 *
 * Every user visible function must have an entry in hyperdex_admin_functions[].
 */
extern zend_function_entry hyperdex_admin_functions[];
/* }}} */


/* {{{ hyperdex_module_entry
 */
zend_module_entry hyperdex_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"hyperdex",
	NULL, /* hyperdex_client_functions,*/
	PHP_MINIT(hyperdex),
	PHP_MSHUTDOWN(hyperdex),
	PHP_RINIT(hyperdex),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(hyperdex),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(hyperdex),
#if ZEND_MODULE_API_NO >= 20010901
	"0.1",                      /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_HYPERDEX
ZEND_GET_MODULE(hyperdex)
#endif

/* {{{ PHP_INI
 */
PHP_INI_BEGIN()
PHP_INI_END()
/* }}} */

/* {{{ php_hyperdex_init_globals
 */
static void php_hyperdex_init_globals(zend_hyperdex_globals *hyperdex_globals)
{

}
/* }}} */

/* {{{ hyperdex_client_storage_bridge
 */
extern void hyperdex_client_free_storage(void *object TSRMLS_DC);
extern zend_object_value hyperdex_client_create_handler(zend_class_entry *type TSRMLS_DC);
extern PHPAPI zend_class_entry *hyperdex_client_get_exception_base( );
extern void hyperdex_client_init_exception(TSRMLS_D);
/* }}} */

/* {{{ hyperdex_admin_storage_bridge
 */
extern void hyperdex_admin_free_storage(void *object TSRMLS_DC);
extern zend_object_value hyperdex_admin_create_handler(zend_class_entry *type TSRMLS_DC);
extern PHPAPI zend_class_entry *hyperdex_admin_get_exception_base( );
extern void hyperdex_admin_init_exception(TSRMLS_D);
/* }}} */


/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION( hyperdex )
{
	zend_class_entry client_ce;
	zend_class_entry admin_ce;

	ZEND_INIT_MODULE_GLOBALS( hyperdex, php_hyperdex_init_globals, NULL );

	REGISTER_INI_ENTRIES();

	INIT_CLASS_ENTRY( client_ce, "HyperdexClient", hyperdex_client_functions );
	hyperdex_client_cmdex_ce = zend_register_internal_class(&client_ce TSRMLS_CC);

	hyperdex_client_cmdex_ce->create_object = hyperdex_client_create_handler;
	memcpy( &hyperdex_client_object_handlers, zend_get_std_object_handlers(), sizeof( zend_object_handlers ) );
	hyperdex_client_object_handlers.clone_obj = NULL;

	hyperdex_client_init_exception( TSRMLS_C );


    INIT_CLASS_ENTRY(admin_ce, "HyperdexAdmin", hyperdex_admin_functions);
    hyperdex_admin_cmdex_ce = zend_register_internal_class(&admin_ce TSRMLS_CC);

    hyperdex_admin_cmdex_ce->create_object = hyperdex_admin_create_handler;
    memcpy(&hyperdex_admin_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    hyperdex_admin_object_handlers.clone_obj = NULL;

    hyperdex_admin_init_exception(TSRMLS_C);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION( hyperdex )
{
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION( hyperdex )
{
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION( hyperdex )
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION( hyperdex )
{
	php_info_print_table_start();
	php_info_print_table_header( 2, "hyperdex support", "enabled" );
	php_info_print_table_header( 2, "hyperdex verion", "1.0.2" );
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	 */
}
/* }}} */



