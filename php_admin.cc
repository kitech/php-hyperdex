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
#include <unistd.h>
//#include <hyperclient.h>
#include <hyperdex/admin.h>
#include <hyperdex.h>
#include <zend_exceptions.h>
#include <zend_operators.h>

// ZEND_DECLARE_MODULE_GLOBALS(hyperdex)

/*
 * Class entries for the hyperclient class and the exception
 */
// zend_class_entry* cmdex_ce;
//zend_class_entry* hyperdex_admin_ce_exception;

//zend_object_handlers hyperdex_admin_object_handlers;
extern zend_class_entry* hyperdex_admin_cmdex_ce;
extern zend_class_entry* hyperdex_admin_ce_exception;

extern zend_object_handlers hyperdex_admin_object_handlers;


/*
 * Allows PHP to store the hyperclient object inbetween calls
 */
// ktodo: dup declare, is this ok
struct hyperdex_admin_object {
	zend_object  std;
	hyperdex_admin* hdex;
	hyperdex_admin_object() : hdex(NULL) {}
};


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
    printf("aaaaaaa111111,%s,%d\n", __FILE__, __LINE__);
}
/* }}} */



/* {{{ proto Boolean __destruct( )
   Disconnect from the HyperDex server, and clean upallocated memory */
PHP_METHOD(HyperdexAdmin, __destruct)
{
}
/* }}} */


/* {{{ proto Boolean add_space(string descript)
   Disconnect from the HyperDex server, and clean upallocated memory */
PHP_METHOD(HyperdexAdmin, add_space)
{
    printf("aaaaaaa111111,%s,%d\n", __FILE__, __LINE__);
    hyperdex_admin *hdex;
	char*                   descript         = NULL;
	int                     descript_len     = -1;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &descript, &descript_len) == FAILURE) {
        RETURN_FALSE;
    }
    printf("aaaaaaa111111,%s,%d, %s\n", __FILE__, __LINE__, descript);

	// Get the hyperclient object from PHP's thread storeage.
	hyperdex_admin_object *obj = (hyperdex_admin_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

    hyperdex_admin_returncode op_status;
    int64_t op_id = hyperdex_admin_add_space(hdex, descript, &op_status);

    printf("aaaaaaa111111,%s,%d, %d\n", __FILE__, __LINE__, op_status);
    
    RETURN_TRUE;
}
/* }}} */

/* {{{ proto Boolean rm_space(string space )
   Disconnect from the HyperDex server, and clean upallocated memory */
PHP_METHOD(HyperdexAdmin, rm_space)
{
    hyperdex_admin *hdex;
    char *space;
    int space_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", space, &space_len) == FAILURE) {
        RETURN_FALSE;
    }

	// Get the hyperclient object from PHP's thread storeage.
	hyperdex_admin_object *obj = (hyperdex_admin_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

    hyperdex_admin_returncode op_status;
    int64_t op_id = hyperdex_admin_rm_space(hdex, space, &op_status);

    hyperdex_admin_returncode lrc;
    int64_t lop_id = hyperdex_admin_loop(hdex, -1, &lrc);

    printf("aaaaaaa111111,%s,%d, %d\n", __FILE__, __LINE__, lrc);
    
    RETURN_TRUE;
}
/* }}} */


/* {{{ proto Array add_space()
   Disconnect from the HyperDex server, and clean upallocated memory */
PHP_METHOD(HyperdexAdmin, list_spaces)
{
    printf("aaaaaaa111111,%s,%d\n", __FILE__, __LINE__);
    hyperdex_admin *hdex;

	// Get the hyperclient object from PHP's thread storeage.
	hyperdex_admin_object *obj = (hyperdex_admin_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

    const char *spaces = NULL;
    hyperdex_admin_returncode op_status;
    int64_t op_id = hyperdex_admin_list_spaces(hdex, &op_status, &spaces);

    printf("aaaaaaa111111,%s,%d, op_id=%d, %d, %p\n", __FILE__, __LINE__, op_id, op_status, spaces);

    hyperdex_admin_returncode lrc;
    int64_t lop_id = hyperdex_admin_loop(hdex, -1, &lrc);

    printf("aaaaaaa111111,%s,%d, op_id=%d, %d, %s\n", __FILE__, __LINE__, op_id, lrc, spaces);

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
    printf("aaaaaaa111111,%s,%d,\n", __FILE__, __LINE__);
    
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

    if (error_message == NULL) {
        RETURN_NULL();
    }

    zval *rval;
    ALLOC_ZVAL(rval);

    RETURN_STRING(error_message, 1);
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

    if (error_location == NULL) {
        RETURN_NULL();
    }

    RETURN_STRING(error_location, 1);
}
/* }}} */
