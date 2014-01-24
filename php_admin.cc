/*
  +----------------------------------------------------------------------+
  | HyperDex admin PHP plugin                                           |
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

/* $Id: php_admin.cc,v 1.5 2013/04/22 10:33:55 shreos Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_hyperdex.h"
#include <unistd.h>
#include <hyperdex/admin.h>
#include <hyperdex.h>
#include <zend_exceptions.h>
#include <zend_operators.h>


/*
 * Class entries for the hyperdex admin class and the exception
 */
zend_class_entry* hyperdex_admin_cmdex_ce;
zend_class_entry* hyperdex_admin_ce_exception;

zend_object_handlers hyperdex_admin_object_handlers;

/*
 * Allows PHP to store the hyperdex admin object inbetween calls
 */
// ktodo: dup declare, is this ok
struct hyperdex_admin_object {
	zend_object  std;
	hyperdex_admin* hdex;
	hyperdex_admin_object() : hdex(NULL) {}
};

/* {{{ hyperdex_admin_functions[]
 *
 * Every user visible function must have an entry in hyperdex_admin_functions[].
 */
// extern zend_function_entry *hyperdex_admin_functions;
/* const static */ zend_function_entry hyperdex_admin_functions[] = {
	PHP_ME(HyperdexAdmin, __construct,                NULL, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR )
	PHP_ME(HyperdexAdmin, __destruct,                 NULL, ZEND_ACC_PUBLIC | ZEND_ACC_DTOR )
	PHP_ME(HyperdexAdmin, dump_config,                 NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexAdmin, read_only,                 NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexAdmin, validate_space,                 NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexAdmin, wait_until_stable,                 NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexAdmin, fault_tolerance,                NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexAdmin, add_space,                 NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexAdmin, rm_space,                 NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexAdmin, list_spaces,                 NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexAdmin, loop,                 NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexAdmin, error_message,                 NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexAdmin, error_location,                 NULL, ZEND_ACC_PUBLIC )

	PHP_FE_END	/* Must be the last line in hyperdex_functions[] */
};
/* }}} */

/* {{{ hyperdex_admin_free_storage
*/
void hyperdex_admin_free_storage(void *object TSRMLS_DC)
{
	hyperdex_admin_object *obj = (hyperdex_admin_object *)object;
	delete obj->hdex;

	zend_hash_destroy( obj->std.properties );
	FREE_HASHTABLE( obj->std.properties );

	efree(obj);
}
/* }}} */

/* {{{ hyperdex_admin_create_handler
*/
zend_object_value hyperdex_admin_create_handler(zend_class_entry *type TSRMLS_DC)
{
	zval *tmp;
	zend_object_value retval;

	hyperdex_admin_object *obj = (hyperdex_admin_object *)emalloc( sizeof( hyperdex_admin_object ) );
	memset( obj, 0, sizeof( hyperdex_admin_object ) );
	obj->std.ce = type;

	ALLOC_HASHTABLE(obj->std.properties);
	zend_hash_init(obj->std.properties, 0, NULL, ZVAL_PTR_DTOR, 0);

#if PHP_VERSION_ID < 50399
	zend_hash_copy( obj->std.properties,
	                &type->default_properties,
	                (copy_ctor_func_t)zval_add_ref, (void *)(&tmp),
	                sizeof(zval *) );
#else
	object_properties_init( &(obj->std), type );
#endif

	retval.handle = zend_objects_store_put( obj, NULL, hyperdex_admin_free_storage, NULL TSRMLS_CC );
	retval.handlers = &hyperdex_admin_object_handlers;

	return retval;
}
/* }}} */


PHPAPI zend_class_entry *hyperdex_admin_get_exception_base( )
{
#if (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 2)
	return zend_exception_get_default();
#else
	return zend_exception_get_default(TSRMLS_C);
#endif
}

void hyperdex_admin_init_exception(TSRMLS_D)
{
	zend_class_entry e;

	INIT_CLASS_ENTRY(e, "HyperdexAdminException", NULL);
	hyperdex_admin_ce_exception = zend_register_internal_class_ex( &e, (zend_class_entry*)hyperdex_admin_get_exception_base(), NULL TSRMLS_CC );
}


/* {{{ proto __construct(string host, int port)
   Construct a HyperDex Admin instance, and connect to a server. */
PHP_METHOD( HyperdexAdmin, __construct )
{
	hyperdex_admin*  hdex        = NULL;
	char*         host        = NULL;
	int           host_len    = -1;
	long          port        = 0;
	zval*         object      = getThis();

	// Get the host name / IP and the port number.
	if (zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "sl", &host, &host_len, &port ) == FAILURE ) {
		RETURN_NULL();
	}

	// Validate them, and throw an exception if there's an issue.
	if( NULL == host || 0 >= host_len ) {
		zend_throw_exception( hyperdex_admin_ce_exception, (char*)"Invalid host", 1001 TSRMLS_CC );
	}
    
	if( 0 >= port || 65536 <= port ) {
		zend_throw_exception( hyperdex_admin_ce_exception, (char*)"Invalid port", 1001 TSRMLS_CC );
	}

	// Now try to create the hyperdex_admin (and connect). Throw an exception if there are errors.
	try {
        hdex = hyperdex_admin_create(host, port);
		if( NULL == hdex ) {
			zend_throw_exception( hyperdex_admin_ce_exception, (char*)"Unable to connect to HyperDex",1 TSRMLS_CC );
		}
	} catch( ... ) {
		hdex = NULL;
		zend_throw_exception( hyperdex_admin_ce_exception, (char*)"Unable to connect to HyperDex",1 TSRMLS_CC );
	}
    
	// If all is good, then set the PHP thread's storage object
	hyperdex_admin_object *obj = (hyperdex_admin_object *)zend_object_store_get_object(object TSRMLS_CC );
	obj->hdex = hdex;
    zend_printf("aaaaaaa111111,%s,%d\n", __FILE__, __LINE__);
}
/* }}} */



/* {{{ proto Boolean __destruct( )
   Disconnect from the HyperDex server, and clean upallocated memory */
PHP_METHOD( HyperdexAdmin, __destruct)
{
}
/* }}} */


/* {{{ proto Array dump_config( )
   Dump hyperdex server configration about attribute and region info */
PHP_METHOD( HyperdexAdmin, dump_config)
{
    hyperdex_admin *hdex;
    hyperdex_admin_object *obj = (hyperdex_admin_object*)zend_object_store_get_object(getThis() TSRMLS_CC);
    hdex = obj->hdex;

    hyperdex_admin_returncode op_status;
    const char *server_config = NULL;
    int64_t op_id = hyperdex_admin_dump_config(hdex, &op_status, &server_config);
    
    hyperdex_admin_returncode lrc;
    int64_t lop_id = hyperdex_admin_loop(hdex, -1, &lrc);

    if (server_config != NULL) {
        zend_printf("aaaaaaa111111,%s,%d, %s, len=%d\n", __FILE__, __LINE__, "server_config", strlen(server_config));
        RETURN_STRING(server_config, 1);
    }
    
    RETURN_FALSE;
}
/* }}} */


/* {{{ proto Boolean read_only( int )
   Disconnect from the HyperDex server, and clean upallocated memory */
PHP_METHOD( HyperdexAdmin, read_only)
{
    hyperdex_admin *hdex;
    long ro;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &ro) == FAILURE) {
        RETURN_FALSE;
    }

	// Get the hyperdex_admin object from PHP's thread storeage.
	hyperdex_admin_object *obj = (hyperdex_admin_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

    hyperdex_admin_returncode op_status;
    int rc = hyperdex_admin_read_only(hdex, ro, &op_status);
    zend_printf("aaaaaaa111111,%s,%d, %d\n", __FILE__, __LINE__, op_status);

    hyperdex_admin_returncode lrc;
    int64_t lop_id = hyperdex_admin_loop(hdex, -1, &lrc);

    if (lrc == HYPERDEX_ADMIN_SUCCESS) {
        RETURN_TRUE;
    }
    RETURN_FALSE;
}
/* }}} */


/* {{{ proto Boolean wait_until_stable()
   Disconnect from the HyperDex server, and clean upallocated memory */
PHP_METHOD( HyperdexAdmin, wait_until_stable)
{
    hyperdex_admin *hdex;

	// Get the hyperdex_admin object from PHP's thread storeage.
	hyperdex_admin_object *obj = (hyperdex_admin_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

    hyperdex_admin_returncode op_status;
    int rc = hyperdex_admin_wait_until_stable(hdex, &op_status);
    zend_printf("aaaaaaa111111,%s,%d, %d\n", __FILE__, __LINE__, op_status);

    hyperdex_admin_returncode lrc;
    int64_t lop_id = hyperdex_admin_loop(hdex, -1, &lrc);

    if (lrc == HYPERDEX_ADMIN_SUCCESS) {
        RETURN_TRUE;
    }
    
    RETURN_FALSE;    
}
/* }}} */


/* {{{ proto Boolean fault_tolerance(String space, uint64_t ft )
   Disconnect from the HyperDex server, and clean upallocated memory */
PHP_METHOD( HyperdexAdmin, fault_tolerance)
{
    hyperdex_admin *hdex;
    char *space = NULL;
    int space_len;
    uint64_t ft;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &space, &space_len, &ft) == FAILURE) {
        RETURN_FALSE;
    }

	// Get the hyperdex_admin object from PHP's thread storeage.
	hyperdex_admin_object *obj = (hyperdex_admin_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

    hyperdex_admin_returncode op_status;
    int rc = hyperdex_admin_fault_tolerance(hdex, space, ft, &op_status);
    zend_printf("aaaaaaa111111,%s,%d, %d\n", __FILE__, __LINE__, op_status);

    hyperdex_admin_returncode lrc;
    int64_t lop_id = hyperdex_admin_loop(hdex, -1, &lrc);
    
    if (lrc == HYPERDEX_ADMIN_SUCCESS) {
        RETURN_TRUE;
    }
    
    RETURN_FALSE;    
}
/* }}} */


/* {{{ proto Boolean validate_space(string descript)
   Disconnect from the HyperDex server, and clean upallocated memory */
PHP_METHOD( HyperdexAdmin, validate_space)
{
    hyperdex_admin *hdex;
	char*                   descript         = NULL;
	int                     descript_len     = -1;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &descript, &descript_len) == FAILURE) {
        RETURN_FALSE;
    }

	// Get the hyperdex_admin object from PHP's thread storeage.
	hyperdex_admin_object *obj = (hyperdex_admin_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

    descript[descript_len] = '\0';
    hyperdex_admin_returncode op_status;
    int rc = hyperdex_admin_validate_space(hdex, descript, &op_status);

    if (rc == 0) {
        RETURN_TRUE;
    }
    RETURN_FALSE;
}
/* }}} */


/* {{{ proto Boolean add_space(string descript)
   Disconnect from the HyperDex server, and clean upallocated memory */
PHP_METHOD( HyperdexAdmin, add_space)
{
    zend_printf("aaaaaaa111111,%s,%d\n", __FILE__, __LINE__);
    hyperdex_admin *hdex;
	char*                   descript         = NULL;
	int                     descript_len     = -1;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &descript, &descript_len) == FAILURE) {
        RETURN_FALSE;
    }
    zend_printf("aaaaaaa111111,%s,%d, %s\n", __FILE__, __LINE__, descript);

	// Get the hyperdex_admin object from PHP's thread storeage.
	hyperdex_admin_object *obj = (hyperdex_admin_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

    hyperdex_admin_returncode op_status;
    int64_t op_id = hyperdex_admin_add_space(hdex, descript, &op_status);

    hyperdex_admin_returncode lrc;
    int64_t lop_id = hyperdex_admin_loop(hdex, -1, &lrc);
    zend_printf("aaaaaaa111111,%s,%d, %d\n", __FILE__, __LINE__, lrc);
    
    if (lrc == HYPERDEX_ADMIN_SUCCESS) {
        RETURN_TRUE;
    }
    
    RETURN_FALSE;
}
/* }}} */

/* {{{ proto Boolean rm_space(string space )
   Disconnect from the HyperDex server, and clean upallocated memory */
PHP_METHOD(HyperdexAdmin, rm_space)
{
    hyperdex_admin *hdex;
    char *space = NULL;
    int space_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &space, &space_len) == FAILURE) {
        RETURN_FALSE;
    }

	// Get the hyperdex admin object from PHP's thread storeage.
	hyperdex_admin_object *obj = (hyperdex_admin_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

    hyperdex_admin_returncode op_status;
    int64_t op_id = hyperdex_admin_rm_space(hdex, space, &op_status);

    hyperdex_admin_returncode lrc;
    int64_t lop_id = hyperdex_admin_loop(hdex, -1, &lrc);

    zend_printf("aaaaaaa111111,%s,%d, %d\n", __FILE__, __LINE__, lrc);
    
    if (lrc == HYPERDEX_ADMIN_SUCCESS) {
        RETURN_TRUE;
    }
    
    RETURN_FALSE;
}
/* }}} */


/* {{{ proto Array add_space()
   Disconnect from the HyperDex server, and clean upallocated memory */
PHP_METHOD(HyperdexAdmin, list_spaces)
{
    zend_printf("aaaaaaa111111,%s,%d\n", __FILE__, __LINE__);
    hyperdex_admin *hdex;

	// Get the hyperdex admin object from PHP's thread storeage.
	hyperdex_admin_object *obj = (hyperdex_admin_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

    const char *spaces = NULL;
    hyperdex_admin_returncode op_status;
    int64_t op_id = hyperdex_admin_list_spaces(hdex, &op_status, &spaces);

    zend_printf("aaaaaaa111111,%s,%d, op_id=%d, %d, %p\n", __FILE__, __LINE__, op_id, op_status, spaces);

    hyperdex_admin_returncode lrc;
    int64_t lop_id = hyperdex_admin_loop(hdex, -1, &lrc);

    zend_printf("aaaaaaa111111,%s,%d, op_id=%d, %d, %s\n", __FILE__, __LINE__, op_id, lrc, spaces);

    zval *rval;
    ALLOC_ZVAL(rval);
    array_init(rval);

    char tmp_spaces[100] = {0};
    strcpy(tmp_spaces, spaces);
    char *prev = tmp_spaces;
    char str[100] = {0};
    for (char *p = tmp_spaces; *p != '\0'; p++) {
        if (*p == '\n') {
            strncpy(str, prev, p - prev);
            str[p - prev] = '\0';
            prev = p + 1;

            zval *nval;
            ALLOC_ZVAL(nval);
            MAKE_STD_ZVAL(nval);
            ZVAL_STRING(nval, str, 1);

            // add_assoc_zval_ex(rval, str, strlen(str), nval);
            add_next_index_zval(rval, nval);
        }
    }
    zend_printf("aaaaaaa111111,%s,%d,\n", __FILE__, __LINE__);
    
    RETURN_ZVAL(rval, 0, 0);
}
/* }}} */

/* {{{ proto Boolean loop( )
   Disconnect from the HyperDex server, and clean upallocated memory */
PHP_METHOD(HyperdexAdmin, loop)
{
    hyperdex_admin *hdex;

    hyperdex_admin_object *obj = (hyperdex_admin_object*)zend_object_store_get_object(getThis() TSRMLS_CC);
    hdex = obj->hdex;

    hyperdex_admin_returncode op_status;
    int64_t op_id = hyperdex_admin_loop(hdex, -1, &op_status);

    if (op_status == HYPERDEX_ADMIN_SUCCESS) {
        RETURN_TRUE;
    }

    RETURN_FALSE;
}
/* }}} */

/* {{{ proto String error_message( )
   Disconnect from the HyperDex server, and clean upallocated memory */
PHP_METHOD(HyperdexAdmin, error_message)
{
    hyperdex_admin *hdex;

    hyperdex_admin_object *obj = (hyperdex_admin_object*)zend_object_store_get_object(getThis() TSRMLS_CC);
    hdex = obj->hdex;

    const char *error_message = hyperdex_admin_error_message(hdex);

    if (error_message != NULL) {
        RETURN_STRING(error_message, 1);
    }

    zval *rval;
    ALLOC_ZVAL(rval);

    RETURN_FALSE;
}
/* }}} */

/* {{{ proto String error_location( )
   Disconnect from the HyperDex server, and clean upallocated memory */
PHP_METHOD(HyperdexAdmin, error_location)
{
    hyperdex_admin *hdex;

    hyperdex_admin_object *obj = (hyperdex_admin_object*)zend_object_store_get_object(getThis() TSRMLS_CC);
    hdex = obj->hdex;

    const char *error_location = hyperdex_admin_error_location(hdex);

    if (error_location != NULL) {
        RETURN_STRING(error_location, 1);
    }

    RETURN_FALSE;
}
/* }}} */
