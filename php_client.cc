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
#include <hyperdex/client.h>
#include <hyperdex.h>
#include <zend_exceptions.h>
#include <zend_operators.h>

/*
 * Class entries for the hyperdex client class and the exception
 */
zend_class_entry* hyperdex_client_cmdex_ce;
zend_class_entry* hyperdex_client_ce_exception;

zend_object_handlers hyperdex_client_object_handlers;


/*
 * Helper functions for HyperDex
 */
int hyperdexClientLoop( hyperdex_client*  hyperclient, int64_t op_id );

uint64_t byteArrayToUInt64 (unsigned char *arr, size_t bytes);
void uint64ToByteArray (uint64_t num, size_t bytes, unsigned char *arr);

void byteArrayToListString( unsigned char* arr, size_t bytes, zval* return_value );
void byteArrayToListInt64( unsigned char* arr, size_t bytes, zval* return_value );
void byteArrayToListFloat( unsigned char* arr, size_t bytes, zval* return_value );

void byteArrayToMapStringString( unsigned char* arr, size_t bytes, zval* return_value );
void byteArrayToMapStringInt64( unsigned char* arr, size_t bytes, zval* return_value );
void byteArrayToMapStringFloat( unsigned char* arr, size_t bytes, zval* return_value );

void byteArrayToMapInt64String( unsigned char* arr, size_t bytes, zval* return_value );
void byteArrayToMapInt64Int64( unsigned char* arr, size_t bytes, zval* return_value );
void byteArrayToMapInt64Float( unsigned char* arr, size_t bytes, zval* return_value );


void stringListToByteArray( zval* array, unsigned char** output_array, size_t *bytes, int sort );
void intListToByteArray( zval* array, unsigned char** output_array, size_t *bytes, int sort );
void doubleListToByteArray( zval* array, unsigned char** output_array, size_t *bytes, int sort );

void stringInt64HashToByteArray( zval* array, unsigned char** output_array, size_t *bytes );
void stringStringHashToByteArray( zval* array, unsigned char** output_array, size_t *bytes );
void stringDoubleHashToByteArray( zval* array, unsigned char** output_array, size_t *bytes );

void int64Int64HashToByteArray( zval* array, unsigned char** output_array, size_t *bytes );
void int64StringHashToByteArray( zval* array, unsigned char** output_array, size_t *bytes );


void buildAttrFromZval( zval* data, char* arr_key, hyperdex_client_attribute* attr, enum hyperdatatype expected_type = HYPERDATATYPE_STRING );
void buildAttrCheckFromZval( zval* data, char* arr_key, hyperdex_client_attribute_check* attr, enum hyperpredicate pred, enum hyperdatatype expected_type= HYPERDATATYPE_STRING);
void buildRangeFromZval( zval* data, char* arr_key, hyperdex_client_attribute_check* fattr, hyperdex_client_attribute_check* sattr);

void buildArrayFromAttrs( hyperdex_client_attribute* attr, size_t attrs_sz, zval* data );
void buildZvalFromAttr( hyperdex_client_attribute* attr, zval* data );

void freeAttrVals( hyperdex_client_attribute *attr, int len );
void freeAttrCheckVals( hyperdex_client_attribute_check *attr, int len );

int isArrayAllLong( zval* array );
int isArrayAllDouble( zval* array );
int isArrayHashTable( zval* array );

char* HyperDexErrorToMsg( hyperdex_client_returncode code );
enum hyperdatatype SimpleBasicType(zval** data); 

static int php_array_string_compare(const void *a, const void *b TSRMLS_DC);
static int php_array_number_compare(const void *a, const void *b TSRMLS_DC);


/*
 * Allows PHP to store the hyperdex_client object inbetween calls
 */
struct hyperdex_client_object {
	zend_object  std;
	hyperdex_client* hdex;
	hyperdex_client_object() : hdex(NULL) {}
};


/* {{{ hyperdex_client_functions[]
 *
 * Every user visible function must have an entry in hyperdex_client_functions[].
 */
// extern zend_function_entry hyperdex_client_functions[];
/* const static*/ zend_function_entry hyperdex_client_functions[] = {
	PHP_ME(HyperdexClient, __construct,                NULL, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR )
	PHP_ME(HyperdexClient, __destruct,                 NULL, ZEND_ACC_PUBLIC | ZEND_ACC_DTOR )
	PHP_ME(HyperdexClient, connect,                 	NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexClient, disconnect,              	NULL, ZEND_ACC_PUBLIC )

	PHP_ME(HyperdexClient, put,                      	NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexClient, put_attr,                 	NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexClient, condput,                  	NULL, ZEND_ACC_PUBLIC )

	PHP_ME(HyperdexClient, lpush,                    	NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexClient, rpush,                    	NULL, ZEND_ACC_PUBLIC )

	PHP_ME(HyperdexClient, set_add,                  	NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexClient, set_remove,                	NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexClient, set_union,                 NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexClient, set_intersect,                 NULL, ZEND_ACC_PUBLIC )

	PHP_ME(HyperdexClient, add,                    	NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexClient, sub,                    	NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexClient, mul,                    	NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexClient, div,                    	NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexClient, mod,                    	NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexClient, and,                    	NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexClient, or,                      	NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexClient, xor,                    	NULL, ZEND_ACC_PUBLIC )

	PHP_ME(HyperdexClient, search,                     NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexClient, get,	                    NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexClient, get_attr,	                NULL, ZEND_ACC_PUBLIC )

	PHP_ME(HyperdexClient, del,                     	NULL, ZEND_ACC_PUBLIC )

	PHP_ME(HyperdexClient, error_message,                     	NULL, ZEND_ACC_PUBLIC )
	PHP_ME(HyperdexClient, error_location,                     	NULL, ZEND_ACC_PUBLIC )

	PHP_FE_END	/* Must be the last line in hyperdex_functions[] */
};
/* }}} */

/* {{{ hyperdex_client_free_storage
*/
void hyperdex_client_free_storage(void *object TSRMLS_DC)
{
	hyperdex_client_object *obj = (hyperdex_client_object *)object;
	delete obj->hdex;

	zend_hash_destroy( obj->std.properties );
	FREE_HASHTABLE( obj->std.properties );

	efree(obj);
}
/* }}} */

/* {{{ hyperdex_client_create_handler
*/
zend_object_value hyperdex_client_create_handler(zend_class_entry *type TSRMLS_DC)
{
	zval *tmp;
	zend_object_value retval;

	hyperdex_client_object *obj = (hyperdex_client_object *)emalloc( sizeof( hyperdex_client_object ) );
	memset( obj, 0, sizeof( hyperdex_client_object ) );
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

	retval.handle = zend_objects_store_put( obj, NULL, hyperdex_client_free_storage, NULL TSRMLS_CC );
	retval.handlers = &hyperdex_client_object_handlers;

	return retval;
}
/* }}} */


PHPAPI zend_class_entry *hyperdex_client_get_exception_base( )
{
#if (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 2)
	return zend_exception_get_default();
#else
	return zend_exception_get_default(TSRMLS_C);
#endif
}

void hyperdex_client_init_exception(TSRMLS_D)
{
	zend_class_entry e;

	INIT_CLASS_ENTRY(e, "HyperdexException", NULL);
	hyperdex_client_ce_exception = zend_register_internal_class_ex( &e, (zend_class_entry*)hyperdex_client_get_exception_base(), NULL TSRMLS_CC );
}


/* {{{ proto __construct(string host, int port)
   Construct a HyperDex Client instance, and connect to a server. */
PHP_METHOD( HyperdexClient, __construct )
{
	hyperdex_client*  hdex        = NULL;
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
		zend_throw_exception( hyperdex_client_ce_exception, (char*)"Invalid host", 1001 TSRMLS_CC );
	}

	if( 0 >= port || 65536 <= port ) {
		zend_throw_exception( hyperdex_client_ce_exception, (char*)"Invalid port", 1001 TSRMLS_CC );
	}

	// Now try to create the hyperdex_client (and connect). Throw an exception if there are errors.
	try {
        hdex = hyperdex_client_create(host, port);
		if( NULL == hdex ) {
			zend_throw_exception( hyperdex_client_ce_exception, (char*)"Unable to connect to HyperDex",1 TSRMLS_CC );
		}
	} catch( ... ) {
		hdex = NULL;
		zend_throw_exception( hyperdex_client_ce_exception, (char*)"Unable to connect to HyperDex",1 TSRMLS_CC );
	}

	// If all is good, then set the PHP thread's storage object
	hyperdex_client_object *obj = (hyperdex_client_object *)zend_object_store_get_object(object TSRMLS_CC );
	obj->hdex = hdex;

}
/* }}} */



/* {{{ proto Boolean __destruct( )
   Disconnect from the HyperDex server, and clean upallocated memory */
PHP_METHOD(HyperdexClient, __destruct)
{
}
/* }}} *


/* {{{ proto Boolean connect(string host, int port)
   Connect to a HyperDex server */
PHP_METHOD( HyperdexClient, connect )
{
	hyperdex_client*  hdex        = NULL;
	char*         host        = NULL;
	int           host_len    = -1;
	long          port        = 0;

	// Get the host name / IP and the port number.
	if( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "sl", &host, &host_len, &port ) == FAILURE ) {
		RETURN_FALSE;
	}

	// Validate them, and throw an exception if there's an issue.
	if( NULL == host || 0 >= host_len ) {
		zend_throw_exception( hyperdex_client_ce_exception, (char*)"Invalid host", 1001 TSRMLS_CC );
		RETURN_FALSE;
	}

	if( 0 >= port || 65536 <= port ) {
		zend_throw_exception( hyperdex_client_ce_exception, (char*)"Invalid port", 1001 TSRMLS_CC );
		RETURN_FALSE;
	}

	// Get the hyperdex_client object from PHP's thread storeage.
	hyperdex_client_object *obj = (hyperdex_client_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

	// If we get it, we need to delete the old one and create the new one.
	if( hdex != NULL ) {
		delete hdex;

		try {

			// Instantiate the hyperdex_client, and try to connect.
            hdex = hyperdex_client_create(host, port);

			if( NULL == hdex ) {
				zend_throw_exception(hyperdex_client_ce_exception, (char*)"Unable to connect to HyperDex",1 TSRMLS_CC);
			}

		} catch( ... ) {
			hdex = NULL;
			zend_throw_exception(hyperdex_client_ce_exception, (char*)"Unable to connect to HyperDex",1 TSRMLS_CC);
		}

		// Push it back to the storage object, and return success.
		obj->hdex = hdex;
	}

	// Return success if we set a valid hyperdex_client
	RETURN_BOOL( hdex != NULL );
}
/* }}} */

/* {{{ proto Boolean disconnect( )
   Disconnect from the HyperDex server */
PHP_METHOD( HyperdexClient, disconnect )
{
	hyperdex_client*        hdex = NULL;
	hyperdex_client_object * obj = (hyperdex_client_object *)zend_object_store_get_object( getThis() TSRMLS_CC );

	hdex = obj->hdex;

	if( NULL != hdex ) {
		delete hdex;
		obj->hdex = NULL;
		RETURN_TRUE;
	}

	RETURN_FALSE;
}
/* }}} */

/* {{{ proto Boolean put(string space, String key, Array attributes)
   Sets one or more attributes for a given space and key. */
PHP_METHOD( HyperdexClient, put )
{
	hyperdex_client*            hdex        = NULL;
	char*                   scope       = NULL;
	int                     scope_len   = -1;
	char*                   key         = NULL;
	int                     key_len     = -1;

	zval                    *arr        = NULL;
	zval                    **data      = NULL;
	HashTable               *arr_hash   = NULL;
	HashPosition            pointer;

	hyperdex_client_attribute*  attr        = NULL;
	int                     attr_cnt    = 0;

	// Get the input parameters: Scope, Key, and Attrs.
	if( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "ssa", &scope, &scope_len, &key, &key_len, &arr ) == FAILURE ) {
		RETURN_FALSE;
	}

	// Set our initial return value to false. Will be set to true if successful.
	RETVAL_TRUE;

	// Get the hyperdex_client object from PHP's thread storeage.
	hyperdex_client_object *obj = (hyperdex_client_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

	// If we have one, perform the operation.
	if( NULL != hdex ) {
		try {

			// Get the array hash for the attributes to be put.
			arr_hash = Z_ARRVAL_P( arr );

			// Allocate the HyperDex attribute array
            attr = new hyperdex_client_attribute[zend_hash_num_elements( arr_hash )];

			// Convert each entry (attribute) in the input array into an element in the hyperdex_client_attribute array.
			for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
			        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
			        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

				char *arr_key;
				unsigned int arr_key_len;
				unsigned long index;

				// Only take attributes (make sure the hash key is a string)
				if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_STRING) {

					// Get the expected type (we may need it)
					hyperdex_client_returncode op_status;
					enum hyperdatatype expected_type = hyperdex_client_attribute_type(hdex, scope, arr_key, &op_status);

					// And convrt the hash value's ZVAL into the Attribute.
					buildAttrFromZval( *data, arr_key, &attr[attr_cnt], expected_type );
					attr_cnt++;
				}
			}

			// Now, call the Hyperdex_Client to do the put of the attribute array.
			hyperdex_client_returncode op_status;

			int64_t op_id = hyperdex_client_put(hdex, scope, key, key_len, attr, attr_cnt, &op_status);

			// Check the return code. Throw an exception and return false if there is an error.
			if( op_id < 0 ) {
				zend_throw_exception( hyperdex_client_ce_exception, HyperDexErrorToMsg( op_status ), op_status TSRMLS_CC );
			} else {
				if( 0 < hyperdexClientLoop( hdex, op_id ) ) {
					RETVAL_TRUE;
				}
			}

		} catch( ... ) {
			zend_throw_exception( hyperdex_client_ce_exception, (char*)"Set failed", 1 TSRMLS_CC );
		}
	}

	// Clean up allocated data
	if( NULL != attr ) {
		freeAttrVals( attr, attr_cnt );
		delete[] attr;
	}

	// And finally return.
	return;
}
/* }}} */

/* {{{ proto Boolean put_attr(string space, string key, string attribute, Mixed value )
       Sets a given attribute for a given record (as defined by key) for a given space. */
PHP_METHOD( HyperdexClient, put_attr )
{
	hyperdex_client*            hdex          = NULL;
	char*                   scope         = NULL;
	int                     scope_len     = -1;
	char*                   key           = NULL;
	int                     key_len       = -1;
	char*                   attr_name     = NULL;
	int                     attr_name_len = -1;

	zval*                   val           = NULL;

	hyperdex_client_attribute*  attr          = NULL;

	// Get the input parameters: Scope, Key, attr name, the value to put there.
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sssz", &scope, &scope_len, &key, &key_len, &attr_name, &attr_name_len, &val) == FAILURE) {
		RETURN_FALSE;
	}

	// Get the hyperdex_client object from PHP's thread storeage.
	hyperdex_client_object *obj = (hyperdex_client_object *)zend_object_store_get_object( getThis() TSRMLS_CC);
	hdex = obj->hdex;

	// Set our intial return value to false (it will be set to true if the operation was successful)
	RETVAL_FALSE;

	// If we have one, perform the operation.
	if( NULL != hdex ) {
		try {

			// Get the expected type (we may need it)
			hyperdex_client_returncode op_status;
			enum hyperdatatype expected_type = hyperdex_client_attribute_type(hdex, scope, key, &op_status);

			// And convert the ZVAL into the hyperdex_client_attribute.
			attr  = new hyperdex_client_attribute();
			buildAttrFromZval( val, attr_name, attr, expected_type );

			// Now, call the Hyperdex_Client to do the put of the attribute array.
			int64_t op_id = hyperdex_client_put(hdex, scope, key, key_len, attr, 1, &op_status);

			// Check the return code. Throw an exception and return false if there is an error.
			if( op_id < 0 ) {
				zend_throw_exception( hyperdex_client_ce_exception, HyperDexErrorToMsg( op_status ), op_status TSRMLS_CC );
			} else {
				if( 0 < hyperdexClientLoop( hdex, op_id ) ) {
					RETVAL_TRUE;
				}
			}

		} catch( ... ) {
			zend_throw_exception( hyperdex_client_ce_exception, (char*)"Set_attr failed", 1 TSRMLS_CC );
		}
	}

	// Clean up allocated memeory
	if( NULL != attr ) {
		freeAttrVals( attr, 1 );
		delete attr;
	}

	// and return.
	return;
}
/* }}} */


/* {{{ proto Boolean condput(string space, string key, Array conditionals, Array attributes)
   Update the attributes for a key if and only if the existing attribute values match the conditionals */
PHP_METHOD( HyperdexClient, condput )
{
	hyperdex_client*            hdex          = NULL;
	char*                   scope         = NULL;
	int                     scope_len     = -1;
	char*                   key           = NULL;
	int                     key_len       = -1;

	zval                    *cond         = NULL;
	zval                    *attr         = NULL;
	zval                    **data        = NULL;
	HashTable               *arr_hash     = NULL;
	HashPosition            pointer;

	hyperdex_client_attribute_check*  cond_attr     = NULL;
	int                     cond_attr_cnt = 0;

	hyperdex_client_attribute*  val_attr      = NULL;
	int                     val_attr_cnt  = 0;

	int                     return_code   = 0;

	// Get the input parameters: Scope, Key, list of precondidtions (array) , the values to put (array).
	if( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "ssaa", &scope, &scope_len, &key, &key_len, &cond, &attr ) == FAILURE ) {
		RETURN_FALSE;
	}

	// Get the hyperdex_client object from PHP's thread storeage.
	hyperdex_client_object *obj = (hyperdex_client_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

	// If we have one, perform the operation.
	if( NULL != hdex ) {
		try {

			// Start by iterating through the conditional array, and building a list of hyperdex_client_attributes from the zvals.
			arr_hash = Z_ARRVAL_P(cond);

			cond_attr = new hyperdex_client_attribute_check[zend_hash_num_elements( arr_hash )];
			cond_attr_cnt = 0;

			for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
			        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
			        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

				char *arr_key;
				unsigned int arr_key_len;
				unsigned long index;

				if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_STRING ) {
					hyperdex_client_returncode op_status;
					enum hyperdatatype expected_type= hyperdex_client_attribute_type(hdex, scope, arr_key, &op_status);
					buildAttrCheckFromZval(*data, arr_key, &cond_attr[cond_attr_cnt], HYPERPREDICATE_EQUALS, expected_type );
					cond_attr_cnt++;
				}
			}

			// Then iterate across the attr array, and build a list of hyperdex_client_attributes from the zvals.
			arr_hash = Z_ARRVAL_P(attr);

			val_attr = new hyperdex_client_attribute[zend_hash_num_elements( arr_hash )];
			val_attr_cnt = 0;

			for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
			        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
			        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

				char *arr_key;
				unsigned int arr_key_len;
				unsigned long index;

				if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_STRING ) {
					hyperdex_client_returncode op_status;
					enum hyperdatatype expected_type= hyperdex_client_attribute_type(hdex, scope, key, &op_status);

					buildAttrFromZval( *data, arr_key, &val_attr[val_attr_cnt], expected_type );
					val_attr_cnt++;
				}
			}


			// Now, perform the operation
			hyperdex_client_returncode op_status;

			int64_t op_id = hyperdex_client_cond_put(hdex, scope, key, key_len, cond_attr, cond_attr_cnt, val_attr, val_attr_cnt, &op_status);
        
			// and check for errors / exception.
			if( op_id < 0 ) {
				zend_throw_exception( hyperdex_client_ce_exception, HyperDexErrorToMsg( op_status ), op_status TSRMLS_CC );
			} else {
				if( 0 < hyperdexClientLoop( hdex, op_id ) ) {

					// Only return TRUE if the operation completed successfully.
					if( op_status == HYPERDEX_CLIENT_SUCCESS ) {
						return_code = 1;
					}

				}

			}

		} catch( ... ) {
			zend_throw_exception( hyperdex_client_ce_exception, (char*)"Set failed", 1 TSRMLS_CC );
		}
	}

	// Free any allocated memory.
	if( NULL != cond_attr ) {
		freeAttrCheckVals( cond_attr, cond_attr_cnt );
		delete[] cond_attr;
	}

	if( NULL != val_attr ) {
		freeAttrVals( val_attr, val_attr_cnt );
		delete[] val_attr;
	}

	// And return success / fail.
	RETURN_BOOL( return_code );
}
/* }}} */


/* {{{ proto Boolean lpush(String space, String key, Array attributes)
   Pushes the specified object to the head of the list of each attribute specified by the request.  */
PHP_METHOD( HyperdexClient, lpush )
{
	hyperdex_client*            hdex        = NULL;
	char*                   scope       = NULL;
	int                     scope_len   = -1;
	char*                   key         = NULL;
	int                     key_len     = -1;

	zval                    *arr        = NULL;
	zval                    **data      = NULL;
	HashTable               *arr_hash   = NULL;
	HashPosition            pointer;

	hyperdex_client_attribute*  attr        = NULL;
	int                     attr_cnt    = 0;

	if( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "ssa", &scope, &scope_len, &key, &key_len, &arr ) == FAILURE ) {
		RETURN_FALSE;
	}

	hyperdex_client_object *obj = (hyperdex_client_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

	RETVAL_FALSE;

	if( NULL != hdex ) {
		try {

			arr_hash = Z_ARRVAL_P(arr);

			attr = new hyperdex_client_attribute[zend_hash_num_elements(arr_hash)];

			for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
			        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
			        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

				char *arr_key;
				unsigned int arr_key_len;
				unsigned long index;

				if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_STRING ) {
					buildAttrFromZval( *data, arr_key, &attr[attr_cnt] );
					attr_cnt++;
				}
			}

			hyperdex_client_returncode op_status;

			int64_t op_id = hyperdex_client_list_lpush(hdex, scope, key, key_len, attr, attr_cnt, &op_status);

			if( op_id < 0 ) {
				zend_throw_exception( hyperdex_client_ce_exception, HyperDexErrorToMsg( op_status ), op_status TSRMLS_CC );
			} else {
				if( 0 < hyperdexClientLoop( hdex, op_id ) ) {
					RETVAL_TRUE;
				}
			}
		} catch( ... ) {
			zend_throw_exception( hyperdex_client_ce_exception, (char*)"lpush failed", 1 TSRMLS_CC );
		}
	}

	if( NULL != attr ) {
		freeAttrVals( attr, attr_cnt );
		delete[] attr;
	}

	return;
}
/* }}} */


/* {{{ proto Boolean rpush(String space, String key, Array attributes)
   Pushes the specified object to the tail of the list of each attribute specified by the request.  */
PHP_METHOD( HyperdexClient, rpush)
{
	hyperdex_client*            hdex        = NULL;
	char*                   scope       = NULL;
	int                     scope_len   = -1;
	char*                   key         = NULL;
	int                     key_len     = -1;

	zval                    *arr        = NULL;
	zval                    **data      = NULL;
	HashTable               *arr_hash   = NULL;
	HashPosition            pointer;

	hyperdex_client_attribute*  attr        = NULL;
	int                     attr_cnt    = 0;

	if( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "ssa", &scope, &scope_len, &key, &key_len, &arr ) == FAILURE ) {
		RETURN_FALSE;
	}

	hyperdex_client_object *obj = (hyperdex_client_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

	RETVAL_FALSE;

	if( NULL != hdex ) {
		try {

			arr_hash = Z_ARRVAL_P(arr);

			attr = new hyperdex_client_attribute[zend_hash_num_elements( arr_hash )];

			for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
			        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
			        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

				char *arr_key;
				unsigned int arr_key_len;
				unsigned long index;

				if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_STRING ) {
					buildAttrFromZval( *data, arr_key, &attr[attr_cnt] );
					attr_cnt++;
				}
			}

			hyperdex_client_returncode op_status;

			int64_t op_id = hyperdex_client_list_rpush(hdex, scope, key, key_len, attr, attr_cnt, &op_status);

			if( op_id < 0 ) {
				zend_throw_exception( hyperdex_client_ce_exception, HyperDexErrorToMsg( op_status ), op_status TSRMLS_CC );
			} else {
				if( 0 < hyperdexClientLoop( hdex, op_id ) ) {
					RETVAL_TRUE;
				}
			}

		} catch( ... ) {
			zend_throw_exception( hyperdex_client_ce_exception, (char*)"lpush failed", 1 TSRMLS_CC );
		}
	}

	if( NULL != attr ) {
		freeAttrVals( attr, attr_cnt );
		delete[] attr;
	}

	return;
}
/* }}} */

/* {{{ proto Boolean set_add(string space, String key, Array attributes)
   Adds a value to one or more sets for a given space and key if the value is not already in the set. */
PHP_METHOD( HyperdexClient, set_add )
{
	hyperdex_client*            hdex        = NULL;
	char*                   scope       = NULL;
	int                     scope_len   = -1;
	char*                   key         = NULL;
	int                     key_len     = -1;

	zval                    *arr        = NULL;
	zval                    **data      = NULL;
	HashTable               *arr_hash   = NULL;
	HashPosition            pointer;

	hyperdex_client_attribute*  attr        = NULL;
	int                     attr_cnt    = 0;

	if( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "ssa", &scope, &scope_len, &key, &key_len, &arr ) == FAILURE ) {
		RETURN_FALSE;
	}

	hyperdex_client_object *obj = (hyperdex_client_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

	RETVAL_FALSE;

	if( NULL != hdex ) {
		try {

			arr_hash = Z_ARRVAL_P( arr );

			attr = new hyperdex_client_attribute[zend_hash_num_elements( arr_hash )];

			for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
			        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
			        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

				char *arr_key;
				unsigned int arr_key_len;
				unsigned long index;

				if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_STRING) {
					buildAttrFromZval( *data, arr_key, &attr[attr_cnt] );
					attr_cnt++;
				}
			}

			hyperdex_client_returncode op_status;

			int64_t op_id = hyperdex_client_set_add(hdex, scope, key, key_len, attr, attr_cnt, &op_status);

			if( op_id < 0 ) {
				zend_throw_exception( hyperdex_client_ce_exception, HyperDexErrorToMsg( op_status ), op_status TSRMLS_CC );
			} else {
				if( 0 < hyperdexClientLoop( hdex, op_id ) ) {
					RETVAL_TRUE;
				}
			}
		} catch( ... ) {
			zend_throw_exception( hyperdex_client_ce_exception, (char*)"set_add failed", 1 TSRMLS_CC );
		}
	}

	if( NULL != attr ) {
		freeAttrVals( attr, attr_cnt );
		delete[] attr;
	}

	return;
}
/* }}} */


/* {{{ proto Boolean set_remove(string space, String key, Array attributes)
   Removes a value to one or more sets for a given space and key if the value is in the set. */
PHP_METHOD( HyperdexClient, set_remove )
{
	hyperdex_client*            hdex        = NULL;
	char*                   scope       = NULL;
	int                     scope_len   = -1;
	char*                   key         = NULL;
	int                     key_len     = -1;

	zval                    *arr        = NULL;
	zval                    **data      = NULL;
	HashTable               *arr_hash   = NULL;
	HashPosition            pointer;

	hyperdex_client_attribute*  attr        = NULL;
	int                     attr_cnt    = 0;

	if( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "ssa", &scope, &scope_len, &key, &key_len, &arr ) == FAILURE ) {
		RETURN_FALSE;
	}

	hyperdex_client_object *obj = (hyperdex_client_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

	RETVAL_FALSE;

	if( NULL != hdex ) {
		try {

			arr_hash = Z_ARRVAL_P( arr );

			attr = new hyperdex_client_attribute[zend_hash_num_elements( arr_hash )];

			for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
			        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
			        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

				char *arr_key;
				unsigned int arr_key_len;
				unsigned long index;

				if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_STRING) {
					buildAttrFromZval( *data, arr_key, &attr[attr_cnt] );
					attr_cnt++;
				}
			}

			hyperdex_client_returncode op_status;

			int64_t op_id = hyperdex_client_set_remove(hdex, scope, key, key_len, attr, attr_cnt, &op_status);

			if( op_id < 0 ) {
				zend_throw_exception( hyperdex_client_ce_exception, HyperDexErrorToMsg( op_status ), op_status TSRMLS_CC );
			} else {
				if( 0 < hyperdexClientLoop( hdex, op_id ) ) {
					RETVAL_TRUE;
				}
			}

		} catch( ... ) {
			zend_throw_exception( hyperdex_client_ce_exception, (char*)"set_add failed", 1 TSRMLS_CC );
		}
	}

	if( NULL != attr ) {
		freeAttrVals( attr, attr_cnt );
		delete[] attr;
	}

	return;
}
/* }}} */


/* {{{ proto Boolean set_union(string space, String key, Array attributes)
   Performs a set union with one or more sets for a given space and key. Sets will be defined by the
   key of the passed in array, with the set to be unioned with the stored set passed as the value.
   The union will be atomic without interference from other operations. */
PHP_METHOD( HyperdexClient, set_union )
{
	hyperdex_client*            hdex        = NULL;
	char*                   scope       = NULL;
	int                     scope_len   = -1;
	char*                   key         = NULL;
	int                     key_len     = -1;

	zval                    *arr        = NULL;
	zval                    **data      = NULL;
	HashTable               *arr_hash   = NULL;
	HashPosition            pointer;

	hyperdex_client_attribute*  attr        = NULL;
	int                     attr_cnt    = 0;

	if( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "ssa", &scope, &scope_len, &key, &key_len, &arr ) == FAILURE ) {
		RETURN_FALSE;
	}

	hyperdex_client_object *obj = (hyperdex_client_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

	RETVAL_FALSE;

	if( NULL != hdex ) {
		try {

			arr_hash = Z_ARRVAL_P( arr );

			attr = new hyperdex_client_attribute[zend_hash_num_elements( arr_hash )];

			for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
			        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
			        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

				char *arr_key;
				unsigned int arr_key_len;
				unsigned long index;

				if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_STRING) {
					hyperdex_client_returncode op_status;
					enum hyperdatatype expected_type = hyperdex_client_attribute_type(hdex, scope, key, &op_status);

					buildAttrFromZval( *data, arr_key, &attr[attr_cnt], expected_type );
					attr_cnt++;
				}
			}

			hyperdex_client_returncode op_status;

			int64_t op_id = hyperdex_client_set_union(hdex, scope, key, key_len, attr, attr_cnt, &op_status);

			if( op_id < 0 ) {
				zend_throw_exception( hyperdex_client_ce_exception, HyperDexErrorToMsg( op_status ), op_status TSRMLS_CC );
			} else {
				if( 0 < hyperdexClientLoop( hdex, op_id ) ) {
					RETVAL_TRUE;
				}
			}

		} catch( ... ) {
			zend_throw_exception( hyperdex_client_ce_exception, (char*)"set_add failed", 1 TSRMLS_CC );
		}
	}

	if( NULL != attr ) {
		freeAttrVals( attr, attr_cnt );
		delete[] attr;
	}

	return;
}
/* }}} */


/* {{{ proto Boolean set_intersect(string space, String key, Array attributes)
   Performs a set intersection with one or more sets for a given space and key. Sets will be defined by the
   key of the passed in array, with the set to be unioned with the stored set passed as the value.
   The union will be atomic without interference from other operations. */
PHP_METHOD( HyperdexClient, set_intersect )
{
	hyperdex_client*            hdex        = NULL;
	char*                   scope       = NULL;
	int                     scope_len   = -1;
	char*                   key         = NULL;
	int                     key_len     = -1;

	zval                    *arr        = NULL;
	zval                    **data      = NULL;
	HashTable               *arr_hash   = NULL;
	HashPosition            pointer;

	hyperdex_client_attribute*  attr        = NULL;
	int                     attr_cnt    = 0;

	if( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "ssa", &scope, &scope_len, &key, &key_len, &arr ) == FAILURE ) {
		RETURN_FALSE;
	}

	hyperdex_client_object *obj = (hyperdex_client_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

	RETVAL_FALSE;

	if( NULL != hdex ) {
		try {

			arr_hash = Z_ARRVAL_P( arr );

			attr = new hyperdex_client_attribute[zend_hash_num_elements( arr_hash )];

			for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
			        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
			        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

				char *arr_key;
				unsigned int arr_key_len;
				unsigned long index;

				if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_STRING) {
					hyperdex_client_returncode op_status;
					enum hyperdatatype expected_type = hyperdex_client_attribute_type(hdex, scope, key, &op_status);

					buildAttrFromZval( *data, arr_key, &attr[attr_cnt], expected_type );
					attr_cnt++;
				}
			}

			hyperdex_client_returncode op_status;

			int64_t op_id = hyperdex_client_set_intersect(hdex, scope, key, key_len, attr, attr_cnt, &op_status);

			if( op_id < 0 ) {
				zend_throw_exception( hyperdex_client_ce_exception, HyperDexErrorToMsg( op_status ), op_status TSRMLS_CC );
			} else {
				if( 0 < hyperdexClientLoop( hdex, op_id ) ) {
					RETVAL_TRUE;
				}
			}

		} catch( ... ) {
			zend_throw_exception( hyperdex_client_ce_exception, (char*)"set_add failed", 1 TSRMLS_CC );
		}
	}

	if( NULL != attr ) {
		freeAttrVals( attr, attr_cnt );
		delete[] attr;
	}

	return;
}
/* }}} */


/* {{{ proto Boolean add(string space, String key, Array attributes)
   Atomically adds integers to one or more attributes for a given space and key. */
PHP_METHOD( HyperdexClient, add )
{
	hyperdex_client*            hdex        = NULL;
	char*                   scope       = NULL;
	int                     scope_len   = -1;
	char*                   key         = NULL;
	int                     key_len     = -1;

	zval                    *arr        = NULL;
	zval                    **data      = NULL;
	HashTable               *arr_hash   = NULL;
	HashPosition            pointer;

	hyperdex_client_attribute*  attr        = NULL;
	int                     attr_cnt    = 0;

	if( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "ssa", &scope, &scope_len, &key, &key_len, &arr ) == FAILURE ) {
		RETURN_FALSE;
	}

	hyperdex_client_object *obj = (hyperdex_client_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

	RETVAL_FALSE;

	if( NULL != hdex ) {
		try {

			arr_hash = Z_ARRVAL_P( arr );

			attr = new hyperdex_client_attribute[zend_hash_num_elements( arr_hash )];

			for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
			        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
			        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

				char *arr_key;
				unsigned int arr_key_len;
				unsigned long index;

				if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_STRING) {
					buildAttrFromZval( *data, arr_key, &attr[attr_cnt] );
					attr_cnt++;
				}
			}

			hyperdex_client_returncode op_status;

			int64_t op_id = hyperdex_client_atomic_add(hdex, scope, key, key_len, attr, attr_cnt, &op_status);

			if( op_id < 0 ) {
				zend_throw_exception( hyperdex_client_ce_exception, HyperDexErrorToMsg( op_status ), op_status TSRMLS_CC );
			} else {
				if( 0 < hyperdexClientLoop( hdex, op_id ) ) {
					RETVAL_TRUE;
				}
			}

		} catch( ... ) {
			zend_throw_exception( hyperdex_client_ce_exception, (char*)"add failed", 1 TSRMLS_CC );
		}
	}

	if( NULL != attr ) {
		freeAttrVals( attr, attr_cnt );
		delete[] attr;
	}

	return;
}
/* }}} */


/* {{{ proto Boolean sub(string space, String key, Array attributes)
   Atomically subtracts integers to one or more attributes for a given space and key. */
PHP_METHOD( HyperdexClient, sub )
{
	hyperdex_client*            hdex        = NULL;
	char*                   scope       = NULL;
	int                     scope_len   = -1;
	char*                   key         = NULL;
	int                     key_len     = -1;

	zval                    *arr        = NULL;
	zval                    **data      = NULL;
	HashTable               *arr_hash   = NULL;
	HashPosition            pointer;

	hyperdex_client_attribute*  attr        = NULL;
	int                     attr_cnt    = 0;

	if( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "ssa", &scope, &scope_len, &key, &key_len, &arr ) == FAILURE ) {
		RETURN_FALSE;
	}

	hyperdex_client_object *obj = (hyperdex_client_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

	RETVAL_FALSE;

	if( NULL != hdex ) {
		try {

			arr_hash = Z_ARRVAL_P( arr );

			attr = new hyperdex_client_attribute[zend_hash_num_elements( arr_hash )];

			for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
			        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
			        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

				char *arr_key;
				unsigned int arr_key_len;
				unsigned long index;

				if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_STRING) {
					buildAttrFromZval( *data, arr_key, &attr[attr_cnt] );
					attr_cnt++;
				}
			}

			hyperdex_client_returncode op_status;

			int64_t op_id = hyperdex_client_atomic_sub(hdex, scope, key, key_len, attr, attr_cnt, &op_status);

			if( op_id < 0 ) {
				zend_throw_exception( hyperdex_client_ce_exception, HyperDexErrorToMsg( op_status ), op_status TSRMLS_CC );
			} else {
				if( 0 < hyperdexClientLoop( hdex, op_id ) ) {
					RETVAL_TRUE;
				}
			}

		} catch( ... ) {
			zend_throw_exception( hyperdex_client_ce_exception, (char*)"sub failed", 1 TSRMLS_CC );
		}
	}

	if( NULL != attr ) {
		freeAttrVals( attr, attr_cnt );
		delete[] attr;
	}

	return;
}
/* }}} */


/* {{{ proto Boolean mul(string space, String key, Array attributes)
   Atomically multiplies integers to one or more attributes for a given space and key. */
PHP_METHOD( HyperdexClient, mul )
{
	hyperdex_client*            hdex        = NULL;
	char*                   scope       = NULL;
	int                     scope_len   = -1;
	char*                   key         = NULL;
	int                     key_len     = -1;

	zval                    *arr        = NULL;
	zval                    **data      = NULL;
	HashTable               *arr_hash   = NULL;
	HashPosition            pointer;

	hyperdex_client_attribute*  attr        = NULL;
	int                     attr_cnt    = 0;

	if( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "ssa", &scope, &scope_len, &key, &key_len, &arr ) == FAILURE ) {
		RETURN_FALSE;
	}

	hyperdex_client_object *obj = (hyperdex_client_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

	RETVAL_FALSE;

	if( NULL != hdex ) {
		try {

			arr_hash = Z_ARRVAL_P( arr );

			attr = new hyperdex_client_attribute[zend_hash_num_elements( arr_hash )];

			for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
			        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
			        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

				char *arr_key;
				unsigned int arr_key_len;
				unsigned long index;

				if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_STRING) {
					buildAttrFromZval( *data, arr_key, &attr[attr_cnt] );
					attr_cnt++;
				}
			}

			hyperdex_client_returncode op_status;

			int64_t op_id = hyperdex_client_atomic_mul(hdex, scope, key, key_len, attr, attr_cnt, &op_status);

			if( op_id < 0 ) {
				zend_throw_exception( hyperdex_client_ce_exception, HyperDexErrorToMsg( op_status ), op_status TSRMLS_CC );
			} else {
				if( 0 < hyperdexClientLoop( hdex, op_id ) ) {
					RETVAL_TRUE;
				}
			}

		} catch( ... ) {
			zend_throw_exception( hyperdex_client_ce_exception, (char*)"mul failed", 1 TSRMLS_CC );
		}
	}

	if( NULL != attr ) {
		freeAttrVals( attr, attr_cnt );
		delete[] attr;
	}

	return;
}
/* }}} */


/* {{{ proto Boolean div(string space, String key, Array attributes)
   Atomically divides the values of one or more attributes for a given space and key by the
   divisor in the input array for that attribute. */
PHP_METHOD( HyperdexClient, div )
{
	hyperdex_client*            hdex        = NULL;
	char*                   scope       = NULL;
	int                     scope_len   = -1;
	char*                   key         = NULL;
	int                     key_len     = -1;

	zval                    *arr        = NULL;
	zval                    **data      = NULL;
	HashTable               *arr_hash   = NULL;
	HashPosition            pointer;

	hyperdex_client_attribute*  attr        = NULL;
	int                     attr_cnt    = 0;

	if( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "ssa", &scope, &scope_len, &key, &key_len, &arr ) == FAILURE ) {
		RETURN_FALSE;
	}

	hyperdex_client_object *obj = (hyperdex_client_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

	RETVAL_FALSE;

	if( NULL != hdex ) {
		try {

			arr_hash = Z_ARRVAL_P( arr );

			attr = new hyperdex_client_attribute[zend_hash_num_elements( arr_hash )];

			for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
			        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
			        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

				char *arr_key;
				unsigned int arr_key_len;
				unsigned long index;

				if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_STRING) {
					buildAttrFromZval( *data, arr_key, &attr[attr_cnt] );
					attr_cnt++;
				}
			}

			hyperdex_client_returncode op_status;

			int64_t op_id = hyperdex_client_atomic_div(hdex, scope, key, key_len, attr, attr_cnt, &op_status);

			if( op_id < 0 ) {
				zend_throw_exception( hyperdex_client_ce_exception, HyperDexErrorToMsg( op_status ), op_status TSRMLS_CC );
			} else {
				if( 0 < hyperdexClientLoop( hdex, op_id ) ) {
					RETVAL_TRUE;
				}
			}

		} catch( ... ) {
			zend_throw_exception( hyperdex_client_ce_exception, (char*)"mul failed", 1 TSRMLS_CC );
		}
	}

	if( NULL != attr ) {
		freeAttrVals( attr, attr_cnt );
		delete[] attr;
	}

	return;
}
/* }}} */


/* {{{ proto Boolean mod(string space, String key, Array attributes)
   Atomically divides the values of one or more attributes for a given space and key by the
   divisor in the input array for that attribute, and stores the remainder. */
PHP_METHOD( HyperdexClient, mod )
{
	hyperdex_client*            hdex        = NULL;
	char*                   scope       = NULL;
	int                     scope_len   = -1;
	char*                   key         = NULL;
	int                     key_len     = -1;

	zval                    *arr        = NULL;
	zval                    **data      = NULL;
	HashTable               *arr_hash   = NULL;
	HashPosition            pointer;

	hyperdex_client_attribute*  attr        = NULL;
	int                     attr_cnt    = 0;

	if( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "ssa", &scope, &scope_len, &key, &key_len, &arr ) == FAILURE ) {
		RETURN_FALSE;
	}

	hyperdex_client_object *obj = (hyperdex_client_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

	RETVAL_FALSE;

	if( NULL != hdex ) {
		try {

			arr_hash = Z_ARRVAL_P( arr );

			attr = new hyperdex_client_attribute[zend_hash_num_elements( arr_hash )];

			for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
			        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
			        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

				char *arr_key;
				unsigned int arr_key_len;
				unsigned long index;

				if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_STRING) {
					buildAttrFromZval( *data, arr_key, &attr[attr_cnt] );
					attr_cnt++;
				}
			}

			hyperdex_client_returncode op_status;

			int64_t op_id = hyperdex_client_atomic_mod(hdex, scope, key, key_len, attr, attr_cnt, &op_status);

			if( op_id < 0 ) {
				zend_throw_exception( hyperdex_client_ce_exception, HyperDexErrorToMsg( op_status ), op_status TSRMLS_CC );
			} else {
				if( 0 < hyperdexClientLoop( hdex, op_id ) ) {
					RETVAL_TRUE;
				}
			}

		} catch( ... ) {
			zend_throw_exception( hyperdex_client_ce_exception, (char*)"mul failed", 1 TSRMLS_CC );
		}
	}

	if( NULL != attr ) {
		freeAttrVals( attr, attr_cnt );
		delete[] attr;
	}

	return;
}
/* }}} */


/* {{{ proto Boolean and(string space, String key, Array attributes)
   Atomically stores the bitwise AND of one or more attributes for a given space and key and
   the integer passed in the input array for that attribute. */
PHP_METHOD( HyperdexClient, and )
{
	hyperdex_client*            hdex        = NULL;
	char*                   scope       = NULL;
	int                     scope_len   = -1;
	char*                   key         = NULL;
	int                     key_len     = -1;

	zval                    *arr        = NULL;
	zval                    **data      = NULL;
	HashTable               *arr_hash   = NULL;
	HashPosition            pointer;

	hyperdex_client_attribute*  attr        = NULL;
	int                     attr_cnt    = 0;

	if( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "ssa", &scope, &scope_len, &key, &key_len, &arr ) == FAILURE ) {
		RETURN_FALSE;
	}

	hyperdex_client_object *obj = (hyperdex_client_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

	RETVAL_FALSE;

	if( NULL != hdex ) {
		try {

			arr_hash = Z_ARRVAL_P( arr );

			attr = new hyperdex_client_attribute[zend_hash_num_elements( arr_hash )];

			for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
			        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
			        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

				char *arr_key;
				unsigned int arr_key_len;
				unsigned long index;

				if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_STRING) {
					buildAttrFromZval( *data, arr_key, &attr[attr_cnt] );
					attr_cnt++;
				}
			}

			hyperdex_client_returncode op_status;

			int64_t op_id = hyperdex_client_atomic_and(hdex, scope, key, key_len, attr, attr_cnt, &op_status);

			if( op_id < 0 ) {
				zend_throw_exception( hyperdex_client_ce_exception, HyperDexErrorToMsg( op_status ), op_status TSRMLS_CC );
			} else {
				if( 0 < hyperdexClientLoop( hdex, op_id ) ) {
					RETVAL_TRUE;
				}
			}

		} catch( ... ) {
			zend_throw_exception( hyperdex_client_ce_exception, (char*)"mul failed", 1 TSRMLS_CC );
		}
	}

	if( NULL != attr ) {
		freeAttrVals( attr, attr_cnt );
		delete[] attr;
	}

	return;
}
/* }}} */


/* {{{ proto Boolean or(string space, String key, Array attributes)
   Atomically stores the bitwise OR of one or more attributes for a given space and key and the
   integer passed in the input array for that attribute. */
PHP_METHOD( HyperdexClient, or )
{
	hyperdex_client*            hdex        = NULL;
	char*                   scope       = NULL;
	int                     scope_len   = -1;
	char*                   key         = NULL;
	int                     key_len     = -1;

	zval                    *arr        = NULL;
	zval                    **data      = NULL;
	HashTable               *arr_hash   = NULL;
	HashPosition            pointer;

	hyperdex_client_attribute*  attr        = NULL;
	int                     attr_cnt    = 0;

	if( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "ssa", &scope, &scope_len, &key, &key_len, &arr ) == FAILURE ) {
		RETURN_FALSE;
	}

	hyperdex_client_object *obj = (hyperdex_client_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

	RETVAL_FALSE;

	if( NULL != hdex ) {
		try {

			arr_hash = Z_ARRVAL_P( arr );

			attr = new hyperdex_client_attribute[zend_hash_num_elements( arr_hash )];

			for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
			        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
			        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

				char *arr_key;
				unsigned int arr_key_len;
				unsigned long index;

				if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_STRING) {
					buildAttrFromZval( *data, arr_key, &attr[attr_cnt] );
					attr_cnt++;
				}
			}

			hyperdex_client_returncode op_status;

			int64_t op_id = hyperdex_client_atomic_or(hdex, scope, key, key_len, attr, attr_cnt, &op_status);

			if( op_id < 0 ) {
				zend_throw_exception( hyperdex_client_ce_exception, HyperDexErrorToMsg( op_status ), op_status TSRMLS_CC );
			} else {
				if( 0 < hyperdexClientLoop( hdex, op_id ) ) {
					RETVAL_TRUE;
				}
			}

		} catch( ... ) {
			zend_throw_exception( hyperdex_client_ce_exception, (char*)"mul failed", 1 TSRMLS_CC );
		}
	}

	if( NULL != attr ) {
		freeAttrVals( attr, attr_cnt );
		delete[] attr;
	}

	return;
}
/* }}} */


/* {{{ proto Boolean xor(string space, String key, Array attributes)
   Atomically stores the bitwise XOR (Exclusive OR of one or more attributes for a given space and key and
   the integer passed in the input array for that attribute. */
PHP_METHOD( HyperdexClient, xor )
{
	hyperdex_client*            hdex        = NULL;
	char*                   scope       = NULL;
	int                     scope_len   = -1;
	char*                   key         = NULL;
	int                     key_len     = -1;

	zval                    *arr        = NULL;
	zval                    **data      = NULL;
	HashTable               *arr_hash   = NULL;
	HashPosition            pointer;

	hyperdex_client_attribute*  attr        = NULL;
	int                     attr_cnt    = 0;

	if( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "ssa", &scope, &scope_len, &key, &key_len, &arr ) == FAILURE ) {
		RETURN_FALSE;
	}

	hyperdex_client_object *obj = (hyperdex_client_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

	RETVAL_FALSE;

	if( NULL != hdex ) {
		try {

			arr_hash = Z_ARRVAL_P( arr );

			attr = new hyperdex_client_attribute[zend_hash_num_elements( arr_hash )];

			for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
			        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
			        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

				char *arr_key;
				unsigned int arr_key_len;
				unsigned long index;

				if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_STRING) {
					buildAttrFromZval( *data, arr_key, &attr[attr_cnt] );
					attr_cnt++;
				}
			}

			hyperdex_client_returncode op_status;

			int64_t op_id = hyperdex_client_atomic_xor(hdex, scope, key, key_len, attr, attr_cnt, &op_status);

			if( op_id < 0 ) {
				zend_throw_exception( hyperdex_client_ce_exception, HyperDexErrorToMsg( op_status ), op_status TSRMLS_CC );
			} else {
				if( 0 < hyperdexClientLoop( hdex, op_id ) ) {
					RETVAL_TRUE;
				}
			}

		} catch( ... ) {
			zend_throw_exception( hyperdex_client_ce_exception, (char*)"mul failed", 1 TSRMLS_CC );
		}
	}

	if( NULL != attr ) {
		freeAttrVals( attr, attr_cnt );
		delete[] attr;
	}

	return;
}
/* }}} */


/* {{{ proto Mixed search(string space, Array equality_conditionals, Array range_conditionals)
   Searches for records with attributes matching the equality_conditionals and the range_conditionals.
   Returns an array of the matching records. */
PHP_METHOD( HyperdexClient, search )
{
	hyperdex_client*             hdex         = NULL;
	char*                    scope        = NULL;
	int                      scope_len    = -1;
	zval*                    eq_cond      = NULL;
	zval*                    range_cond   = NULL;

	zval**                   data;
	HashTable*               eq_arr_hash;
	HashPosition             eq_pointer;
	HashTable*               rn_arr_hash;
	HashPosition             rn_pointer;

	hyperdex_client_attribute_check*   cond_attr = NULL;
	int                      cond_attr_cnt = 0;

	if( zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "saa", &scope, &scope_len, &eq_cond, &range_cond ) == FAILURE ) {
		RETURN_FALSE;
	}

	hyperdex_client_object *obj = (hyperdex_client_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

	if( NULL != hdex ) {
		try {

			//
			// Get the Equality Test Attributes...
			//
			eq_arr_hash = Z_ARRVAL_P(eq_cond);
			rn_arr_hash = Z_ARRVAL_P(range_cond);

			cond_attr = new hyperdex_client_attribute_check[
				zend_hash_num_elements(eq_arr_hash) + 
				(2*zend_hash_num_elements(rn_arr_hash))
			];
			cond_attr_cnt = 0;

			for( zend_hash_internal_pointer_reset_ex( eq_arr_hash, &eq_pointer );
			        zend_hash_get_current_data_ex( eq_arr_hash, (void**) &data, &eq_pointer ) == SUCCESS;
			        zend_hash_move_forward_ex( eq_arr_hash, &eq_pointer ) ) {

				char *arr_key;
				unsigned int arr_key_len;
				unsigned long index;

				if( zend_hash_get_current_key_ex(eq_arr_hash, &arr_key, &arr_key_len, &index, 0, &eq_pointer ) == HASH_KEY_IS_STRING ) {
					hyperdex_client_returncode op_status;
					enum hyperdatatype expected_type = hyperdex_client_attribute_type(hdex, scope, arr_key, &op_status);
					buildAttrCheckFromZval(*data, arr_key, &cond_attr[cond_attr_cnt], HYPERPREDICATE_EQUALS, expected_type );
					++cond_attr_cnt;
				}
			}

			//
			// Get the Range Test Attributes...
			//
			for( zend_hash_internal_pointer_reset_ex( rn_arr_hash, &rn_pointer );
			        zend_hash_get_current_data_ex( rn_arr_hash, (void**) &data, &rn_pointer ) == SUCCESS;
			        zend_hash_move_forward_ex( rn_arr_hash, &rn_pointer ) ) {

				char *arr_key;
				unsigned int arr_key_len;
				unsigned long index;

				if( zend_hash_get_current_key_ex( rn_arr_hash, &arr_key, &arr_key_len, &index, 0, &rn_pointer ) == HASH_KEY_IS_STRING ) {
					buildRangeFromZval(*data, arr_key, &cond_attr[cond_attr_cnt], &cond_attr[cond_attr_cnt+1] );
					cond_attr_cnt+=2;
				}
			}

			//
			// Build the query...
			//
			hyperdex_client_returncode op_status;
			hyperdex_client_attribute* attrs = NULL;
			size_t attrs_sz = 0;

			int64_t op_id = hyperdex_client_search(hdex, scope, cond_attr, cond_attr_cnt, &op_status, (const hyperdex_client_attribute**)&attrs, &attrs_sz);

			if( op_id < 0 ) {
				freeAttrCheckVals( cond_attr, cond_attr_cnt );
				zend_throw_exception( hyperdex_client_ce_exception, HyperDexErrorToMsg( op_status ), op_status TSRMLS_CC );
			} else {

				array_init(return_value);

				while( 1 == hyperdexClientLoop( hdex, op_id ) && HYPERDEX_CLIENT_SEARCHDONE != op_status ) {
					if( attrs_sz > 0 ) {
						zval *temp;
						ALLOC_INIT_ZVAL(temp);

						buildArrayFromAttrs( attrs, attrs_sz, temp );

						add_next_index_zval( return_value, temp );
					}
				}

				hyperdex_client_destroy_attrs( attrs, attrs_sz );

				if( NULL != cond_attr ) {
					freeAttrCheckVals( cond_attr, cond_attr_cnt );
					delete[] cond_attr;
				}
				return;
			}

		} catch( int& e ) {
			zend_throw_exception( hyperdex_client_ce_exception, (char*)"Bad Range Check", 1 TSRMLS_CC );
		} catch( ... ) {
			zend_throw_exception( hyperdex_client_ce_exception, (char*)"Search failed", 1 TSRMLS_CC );
		}
	}

	if( NULL != cond_attr ) {
		freeAttrCheckVals( cond_attr, cond_attr_cnt );
		delete[] cond_attr;
	}

	RETURN_FALSE;
}

/* }}} */

/* {{{ proto Array get(string space, string key)
   Returns the record identified by key from the specified space (table) */
PHP_METHOD( HyperdexClient, get )
{
	hyperdex_client*  hdex        = NULL;
	char*         scope       = NULL;
	int           scope_len   = -1;
	char*         key         = NULL;
	int           key_len     = -1;
	int           val_len     = -1;

	if( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "ss", &scope, &scope_len, &key, &key_len ) == FAILURE ) {
		RETURN_FALSE;
	}

	hyperdex_client_object *obj = (hyperdex_client_object *)zend_object_store_get_object( getThis() TSRMLS_CC);
	hdex = obj->hdex;

	if( NULL != hdex ) {
		try {

			hyperdex_client_returncode op_status;
			hyperdex_client_attribute* attrs = NULL;
			size_t attrs_sz = 0;

			int64_t op_id = hyperdex_client_get(hdex, scope, key, key_len, &op_status, (const hyperdex_client_attribute**)&attrs, &attrs_sz);

			if( op_id < 0 ) {
				zend_throw_exception( hyperdex_client_ce_exception, HyperDexErrorToMsg( op_status ), op_status TSRMLS_CC );
			} else {

				if( 1 == hyperdexClientLoop( hdex, op_id ) ) {

					buildArrayFromAttrs( attrs, attrs_sz, return_value );

					hyperdex_client_destroy_attrs( attrs, attrs_sz );

					return;
				}
			}
		} catch( ... ) {
			zend_throw_exception( hyperdex_client_ce_exception, (char*)"get failed", 1 TSRMLS_CC );
		}
	}
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto Mixed get(string space, string key, string attribute)
   Returns the named attribute from the record identified by key from the specified space (table) */
PHP_METHOD( HyperdexClient, get_attr )
{
	hyperdex_client*  hdex          = NULL;
	char*         scope         = NULL;
	int           scope_len     = -1;
	char*         key           = NULL;
	int           key_len       = -1;
	char*         attr_name     = NULL;
	int           attr_name_len = -1;

	if( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "sss", &scope, &scope_len, &key, &key_len, &attr_name, &attr_name_len ) == FAILURE ) {
		RETURN_FALSE;
	}

	hyperdex_client_object *obj = (hyperdex_client_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

	if( NULL != hdex ) {
		try {

			hyperdex_client_returncode op_status;
			hyperdex_client_attribute* attrs = NULL;
			size_t attrs_sz = 0;

			int64_t op_id = hyperdex_client_get(hdex, scope, key, key_len, &op_status, (const hyperdex_client_attribute**)&attrs, &attrs_sz);

			if( op_id < 0 ) {
				zend_throw_exception( hyperdex_client_ce_exception, HyperDexErrorToMsg( op_status ), op_status TSRMLS_CC );
			} else {

				if( 1 == hyperdexClientLoop( hdex, op_id ) ) {

					RETVAL_FALSE;

					if( attrs_sz > 0 ) {
						for( int i = 0; i < attrs_sz; ++i ) {

							if( strlen( attrs[i].attr ) == attr_name_len && strncmp( attrs[i].attr, attr_name, attrs[i].value_sz ) == 0 ) {
								buildZvalFromAttr( &attrs[i], return_value );
								break;
							}

						}
					}

					hyperdex_client_destroy_attrs( attrs, attrs_sz );
					return;
				}
			}
		} catch( ... ) {
			zend_throw_exception( hyperdex_client_ce_exception, (char*)"get failed", 1 TSRMLS_CC );
		}
	}

	RETURN_FALSE;
}
/* }}} */

/* {{{ proto Boolean del(string space, string key)
   Delete the record identified by key from the specified space */
PHP_METHOD( HyperdexClient, del )
{
	hyperdex_client*  hdex        = NULL;
	char*         scope       = NULL;
	int           scope_len   = -1;
	char*         key         = NULL;
	int           key_len     = -1;

	if( zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "ss", &scope, &scope_len, &key, &key_len ) == FAILURE ) {
		RETURN_FALSE;
	}

	hyperdex_client_object *obj = (hyperdex_client_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

	if( NULL != hdex ) {
		try {

			hyperdex_client_returncode op_status;
			hyperdex_client_attribute* attrs = NULL;
			size_t attrs_sz = 0;

			int64_t op_id = hyperdex_client_del(hdex, scope, key, key_len, &op_status);

			if( op_id < 0 ) {
				zend_throw_exception(hyperdex_client_ce_exception, HyperDexErrorToMsg( op_status ), op_status TSRMLS_CC);
			} else {
				if( 1 == hyperdexClientLoop( hdex, op_id ) ) {
					RETURN_TRUE;
				}
			}

		} catch( ... ) {
			zend_throw_exception(hyperdex_client_ce_exception, (char*)"delete failed", 1 TSRMLS_CC);
		}
	}

	RETURN_FALSE;
}
/* }}} */

/* {{{ proto String error_message()
   Get error message info about last hyperdex client operation */
PHP_METHOD( HyperdexClient, error_message )
{
	hyperdex_client*  hdex        = NULL;
	hyperdex_client_object *obj = (hyperdex_client_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

	if( NULL != hdex ) {
		try {
            const char *error_message = hyperdex_client_error_message(hdex);
            if (error_message != NULL) {
                RETURN_STRING(error_message, 1);
            }
		} catch( ... ) {
			zend_throw_exception(hyperdex_client_ce_exception, (char*)"get error message failed", 1 TSRMLS_CC);
		}
	}

	RETURN_FALSE;
}
/* }}} */

/* {{{ proto Boolean error_location()
   Get error location info about last hyperdex client operation */
PHP_METHOD( HyperdexClient, error_location )
{
	hyperdex_client*  hdex        = NULL;
	hyperdex_client_object *obj = (hyperdex_client_object *)zend_object_store_get_object( getThis() TSRMLS_CC );
	hdex = obj->hdex;

	if( NULL != hdex ) {
		try {
            const char *error_location = hyperdex_client_error_location(hdex);
            if (error_location != NULL) {
                RETURN_STRING(error_location, 1);
            }
		} catch( ... ) {
			zend_throw_exception(hyperdex_client_ce_exception, (char*)"get error location failed", 1 TSRMLS_CC);
		}
	}

	RETURN_FALSE;
}
/* }}} */


/**
 * Call the HyperDex receive loop so that we can get our data.
 * Checks the return code from loop for errors, and throws the necessary
 * exceptions (based on loop status) if there is an error.
 */
int hyperdexClientLoop( hyperdex_client* hdex, int64_t op_id )
{
	hyperdex_client_returncode loop_status;

	int64_t loop_id = hyperdex_client_loop(hdex, -1, &loop_status);

	if( loop_id < 0 ) {
		if( HYPERDEX_CLIENT_NONEPENDING != loop_status ) {
			zend_throw_exception( hyperdex_client_ce_exception, HyperDexErrorToMsg( loop_status ), loop_status TSRMLS_CC );
			return loop_status;
		}

		return -1;
	}

	if( op_id != loop_id ) {
		// This is most certainly a bug for this simple code.
		zend_throw_exception( hyperdex_client_ce_exception, (char*)"Loop ID not equal Op ID", 2 TSRMLS_CC );
		return -1;
	}

	return 1;
}


/**
 *  Converts a HyperDex returned byte array (little endian) Into a 64 bit long integer (regardless of platform)
 */
uint64_t byteArrayToUInt64( unsigned char *arr, size_t bytes )
{
	uint64_t num = 0ul;
	uint64_t tmp;

	size_t i;
	for( i = 0; i < bytes; i++ ) {
		tmp = 0ul;
		tmp = arr[i];
		num |= (tmp << ((i & 7) << 3));
	}
	return num;
}


/**
 *  Converts a 64 bit long integer into a HyperDex returned byte array (little endian) (regardless of platform)
 */
void uint64ToByteArray( uint64_t num, size_t bytes, unsigned char *arr )
{
	size_t        i;
	unsigned char ch;

	for( i = 0; i < bytes; i++  ) {
		ch = (num >> ((i & 7) << 3)) & 0xFF;
		arr[i] = ch;
	}
}


/**
 *  Converts a HyperDex returned byte array into a PHP array of C strings. ( Handles multibyte and long strings )
 */
void byteArrayToListString( unsigned char* arr, size_t bytes, zval* return_value )
{
	size_t cntr = 0;

	array_init( return_value );

	while( cntr < bytes ) {
		uint64_t byteCount = byteArrayToUInt64( arr + cntr, 4 );

		char* string = (char*)ecalloc( 1, byteCount+1 );
		memcpy( string, arr + cntr+4, byteCount );

		add_next_index_stringl( return_value, string, byteCount, 0 );

		cntr = cntr + 4 + byteCount;
	}
}


/**
 *  Converts a HyperDex returned byte array (little endian) Into a PHP array of 64 bit long integers (regardless of platform)
 */
void byteArrayToListInt64( unsigned char* arr, size_t bytes, zval* return_value )
{
	size_t cntr = 0;

	array_init(return_value);

	while( cntr < bytes ) {
		uint64_t val = byteArrayToUInt64( arr + cntr, 8 );
		add_next_index_long( return_value, val );
		cntr = cntr + 8;
	}
}


/**
 *  Converts a HyperDex returned byte array (IEEE 754) Into a PHP array of doubles
 */
void byteArrayToListFloat( unsigned char* arr, size_t bytes, zval* return_value )
{
	size_t cntr = 0;

	array_init(return_value);

	while( cntr < bytes ) {
		add_next_index_double( return_value, *( (double*)( arr + cntr ) ) );
		cntr = cntr + sizeof(double);
	}
}

/**
 *  Converts a HyperDex returned byte array into a PHP Map of C strings. ( Handles multibyte and long strings for value, not key.)
 *  (key = string, val = string)
 */
void byteArrayToMapStringString( unsigned char* arr, size_t bytes, zval* return_value )
{
	size_t cntr = 0;

	array_init( return_value );

	while( cntr < bytes ) {

		uint64_t byteCount = byteArrayToUInt64( arr + cntr, 4 );
		char* key = (char*)ecalloc( 1, byteCount+1 );
		memcpy( key, arr + cntr+4, byteCount );
		cntr = cntr + 4 + byteCount;

		byteCount = byteArrayToUInt64( arr + cntr, 4 );
		char* val = (char*)ecalloc( 1, byteCount+1 );
		memcpy( val, arr + cntr+4, byteCount );
		cntr = cntr + 4 + byteCount;

		add_assoc_stringl( return_value, key, val, byteCount, 0 );
	}
}


/**
 *  Converts a HyperDex returned byte array into a PHP Map of Long Integers. (key = string, val = integer)
 */
void byteArrayToMapStringInt64( unsigned char* arr, size_t bytes, zval* return_value )
{
	size_t cntr = 0;

	array_init( return_value );

	while( cntr < bytes ) {

		uint64_t byteCount = byteArrayToUInt64( arr + cntr, 4 );
		char* key = (char*)ecalloc( 1, byteCount+1 );
		memcpy( key, arr + cntr+4, byteCount );
		cntr = cntr + 4 + byteCount;

		uint64_t val = byteArrayToUInt64( arr + cntr, 8 );
		cntr = cntr + 8;

		add_assoc_long( return_value, key, val );

	}
}


/**
 *  Converts a HyperDex returned byte array into a PHP Map of Double floats. (key = string, val = double)
 */
void byteArrayToMapStringFloat( unsigned char* arr, size_t bytes, zval* return_value )
{
	size_t cntr = 0;

	array_init( return_value );

	while( cntr < bytes ) {

		uint64_t byteCount = byteArrayToUInt64( arr + cntr, 4 );

		char* key = (char*)ecalloc( 1, byteCount+1 );
		memcpy( key, arr + cntr+4, byteCount );
		cntr = cntr + 4 + byteCount;

		double val = *( (double*)( arr + cntr ));
		cntr = cntr + sizeof( double );

		add_assoc_double( return_value, key, val );

	}
}


/**
 *  Converts a HyperDex returned byte array (little endian) Into a PHP map of strings with index as a number (regardless of platform)
 *  (key = integer, val = string)
 */
void byteArrayToMapInt64String( unsigned char* arr, size_t bytes, zval* return_value )
{
	size_t cntr = 0;

	array_init(return_value);

	while( cntr < bytes ) {

		uint64_t key_val = byteArrayToUInt64( arr + cntr, 8 );
		char* key = (char*)ecalloc( 1, 16 );
		sprintf(key, "%ld", key_val );
		cntr = cntr + 8;

		uint64_t byteCount = byteArrayToUInt64( arr + cntr, 4 );
		char* val = (char*)ecalloc( 1, byteCount+1 );
		memcpy( val, arr + cntr+4, byteCount );
		cntr = cntr + 4 + byteCount;

		add_assoc_stringl( return_value, key, val, byteCount, 0 );
	}
}


/**
 *  Converts a HyperDex returned byte array into a PHP Map of Long integers. (key = integer, val = integer)
 */
void byteArrayToMapInt64Int64( unsigned char* arr, size_t bytes, zval* return_value )
{
	size_t cntr = 0;

	array_init( return_value );

	while( cntr < bytes ) {

		uint64_t key_val = byteArrayToUInt64( arr + cntr, 8 );
		char* key = (char*)ecalloc( 1, 16 );
		sprintf(key, "%ld", key_val );
		cntr = cntr + 8;

		uint64_t val = byteArrayToUInt64( arr + cntr, 8 );
		cntr = cntr + 8;

		add_assoc_long( return_value, key, val );
	}
}

/**
 *  Converts a HyperDex returned byte array into a PHP Map of Double precision floats. (key = integer, val = double)
 */
void byteArrayToMapInt64Float( unsigned char* arr, size_t bytes, zval* return_value )
{
	size_t cntr = 0;

	array_init( return_value );

	while( cntr < bytes ) {

		uint64_t key_val = byteArrayToUInt64( arr + cntr, 8 );

		char* key = (char*)ecalloc( 1, 16 );
		sprintf(key, "%ld", key_val );
		cntr = cntr + 8;

		double val = *( (double*) arr + cntr );
		cntr = cntr + sizeof( double );

		add_assoc_double( return_value, key, val );
	}
}


/**
 * Converts a PHP list of strings (multibyte safe) into a Byte Array suitable for HyperDex to store.
 */
void stringListToByteArray( zval* array, unsigned char** output_array, size_t *bytes, int sort )
{
	HashTable*   arr_hash = NULL;
	HashPosition pointer;
	zval**       data     = NULL;

	// Create a copy of the zval, so we don't overwrite the order of the original if we're sorting.
	zval temp_array = *array;
	zval_copy_ctor( &temp_array );
	arr_hash = Z_ARRVAL( temp_array );

	// Sort the list if we need to
	if( sort ) {
		zend_hash_sort ( arr_hash, zend_qsort, php_array_string_compare, 1 TSRMLS_CC );
	}


	int array_count = zend_hash_num_elements( arr_hash );

	// Start by getting the overall size of the memory block that we will need to allocate.
	size_t byteCount = 0;
	for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
	        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
	        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

		zval temp = **data;
		zval_copy_ctor( &temp );
		convert_to_string( &temp );

		byteCount = byteCount + Z_STRLEN( temp ) + 4;
	}

	*bytes = byteCount;

	// Allocate it
	*output_array = (unsigned char*)calloc( 1, byteCount + 1 );

	// Now, fill it with the data that we want.2420
	size_t ptr = 0;
	for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
	        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
	        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

		zval temp = **data;
		zval_copy_ctor( &temp );
		convert_to_string( &temp );

		// set the size of the current string element.
		uint64ToByteArray( Z_STRLEN( temp ) , 4, (*output_array) + ptr );

		// copy the string data
		memcpy( (*output_array) + 4 + ptr, Z_STRVAL( temp ), Z_STRLEN( temp ) );

		// move to the next string starting position
		ptr = ptr + 4 + Z_STRLEN( temp );
	}
}

/**
 * Converts a PHP list of integers (platform independent) into a Byte Array (little endian) suitable for HyperDex to store.
 */
void intListToByteArray( zval* array, unsigned char** output_array, size_t *bytes, int sort )
{
	HashTable*    arr_hash = NULL;
	HashPosition  pointer;
	zval**        data     = NULL;

	// Create a copy of the zval, so we don't overwrite the order of the original if we're sorting.
	zval temp_array = *array;
	zval_copy_ctor( &temp_array );
	arr_hash = Z_ARRVAL( temp_array );

	// Sort the list if we need to
	if( sort ) {
		zend_hash_sort ( arr_hash, zend_qsort, php_array_number_compare, 1 TSRMLS_CC );
	}

	int array_count = zend_hash_num_elements( arr_hash );

	// Start by getting the overall size of the memory block that we will need to allocate.
	size_t byteCount = 8 * array_count;

	*bytes = byteCount;

	// Allocate it
	*output_array = (unsigned char*)calloc( 1, byteCount + 1 );

	// Now, fill it with the data that we want.
	size_t ptr = 0;
	for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
	        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
	        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

		if( Z_TYPE_PP( data ) == IS_LONG ) {

			// convert the long into an 8 byte array (little-endian) and store it
			uint64ToByteArray( Z_LVAL_PP( data ) , 8, *output_array + ptr );

			// move to the next position
			ptr = ptr + 8;
		}
	}
}


/**
 * Converts a PHP list of doubles (IEEE 754) into a Byte Array suitable for HyperDex to store.
 */
void doubleListToByteArray( zval* array, unsigned char** output_array, size_t *bytes, int sort )
{
	HashTable*    arr_hash = NULL;
	HashPosition  pointer;
	zval**        data     = NULL;

	// Create a copy of the zval, so we don't overwrite the order of the original if we're sorting.
	zval temp_array = *array;
	zval_copy_ctor( &temp_array );
	arr_hash = Z_ARRVAL( temp_array );

	// Sort the list if we need to
	if( sort ) {
		zend_hash_sort ( arr_hash, zend_qsort, php_array_number_compare, 1 TSRMLS_CC );
	}

	int array_count = zend_hash_num_elements( arr_hash );

	// Start by getting the overall size of the memory block that we will need to allocate.
	size_t byteCount = sizeof(double) * array_count;

	*bytes = byteCount;

	// Allocate it
	*output_array = (unsigned char*)calloc( 1, byteCount + 1 );

	// Now, fill it with the data that we want.
	size_t ptr = 0;
	for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
	        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
	        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

		if( Z_TYPE_PP( data ) == IS_DOUBLE ) {

			// convert the double into a byte array and store it
			memcpy( *output_array + ptr, (double*)&Z_DVAL_PP( data ), sizeof(double) );

			// move to the next position
			ptr = ptr + sizeof(double);
		}
	}
}


/**
 * Converts a PHP hash of strings into a Byte Array suitable for HyperDex to store.
 */
void stringStringHashToByteArray( zval* array, unsigned char** output_array, size_t *bytes )
{
	HashTable*     arr_hash = NULL;
	HashPosition   pointer;
	char*            arr_key;
	unsigned int    arr_key_len;
	unsigned long   index;

	zval**        data     = NULL;

	arr_hash = Z_ARRVAL_P( array );
	int array_count = zend_hash_num_elements( arr_hash );

	// Start by getting the overall size of the memory block that we will need to allocate.
	size_t byteCount = 0;
	for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
	        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
	        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

		if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_STRING) {
			byteCount = byteCount + arr_key_len + 4;

			zval temp = **data;
			zval_copy_ctor( &temp );
			convert_to_string( &temp );

			byteCount = byteCount + Z_STRLEN( temp ) + 4;
		}
	}

	*bytes = byteCount;

	// Allocate it
	*output_array = (unsigned char*)calloc( 1, byteCount + 1 );

	// Now, fill it with the data that we want.
	size_t ptr = 0;
	for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
	        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
	        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

		if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_STRING) {

			// set the size of the current string key.
			uint64ToByteArray( arr_key_len , 4, (*output_array) + ptr );

			// copy the string data
			memcpy( (*output_array) + 4 + ptr, arr_key, arr_key_len );

			// move to the next string starting position
			ptr = ptr + 4 + arr_key_len;

			//
			// Now, make sure the data element for this entry is a string
			//
			zval temp = **data;
			zval_copy_ctor( &temp );
			convert_to_string( &temp );

			// set the size of the current string element.
			uint64ToByteArray( Z_STRLEN( temp ) , 4, (*output_array) + ptr );

			// copy the string data
			memcpy( (*output_array) + 4 + ptr, Z_STRVAL( temp ), Z_STRLEN( temp ) );

			// move to the next string starting position
			ptr = ptr + 4 + Z_STRLEN( temp );
		}
	}
}

/**
 * Converts a PHP hash of Integers ( key=string, value=integer ) into a Byte Array suitable for HyperDex to store.
 */
void stringInt64HashToByteArray( zval* array, unsigned char** output_array, size_t *bytes )
{
	HashTable*       arr_hash = NULL;
	HashPosition     pointer;
	char*            arr_key;
	unsigned int    arr_key_len;
	unsigned long   index;
	zval**           data     = NULL;

	arr_hash = Z_ARRVAL_P( array );
	int array_count = zend_hash_num_elements( arr_hash );

	// Start by getting the overall size of the memory block that we will need to allocate.
	size_t byteCount = 0;
	for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
	        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
	        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {
		if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_STRING) {
			byteCount = byteCount + arr_key_len + 4 + 8;
		}
	}

	*bytes = byteCount;

	// Allocate it
	*output_array = (unsigned char*)calloc( 1, byteCount + 1 );

	// Now, fill it with the data that we want.
	size_t ptr = 0;
	for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
	        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
	        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

		if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_STRING) {

			// set the size of the current string key.
			uint64ToByteArray( arr_key_len , 4, (*output_array) + ptr );

			// copy the string data
			memcpy( (*output_array) + 4 + ptr, arr_key, arr_key_len );

			// move to the next string starting position
			ptr = ptr + 4 + arr_key_len;

			// The data is an int64, so convert it
			uint64ToByteArray( Z_LVAL_PP(data) , 8, *output_array + ptr );

			// move to the next position
			ptr = ptr + 8;
		}
	}
}


/**
 * Converts a PHP hash of Doubles ( key=string, value=double ) into a Byte Array suitable for HyperDex to store.
 */
void stringDoubleHashToByteArray( zval* array, unsigned char** output_array, size_t *bytes )
{
	HashTable*       arr_hash = NULL;
	HashPosition     pointer;
	char*            arr_key;
	unsigned int    arr_key_len;
	unsigned long   index;
	zval**           data     = NULL;

	arr_hash = Z_ARRVAL_P( array );
	int array_count = zend_hash_num_elements( arr_hash );

	// Start by getting the overall size of the memory block that we will need to allocate.
	size_t byteCount = 0;
	for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
	        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
	        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {
		if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_STRING) {
			byteCount = byteCount + arr_key_len + 4 + sizeof( double);
		}
	}

	*bytes = byteCount;

	// Allocate it
	*output_array = (unsigned char*)calloc( 1, byteCount + 1 );

	// Now, fill it with the data that we want.
	size_t ptr = 0;
	for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
	        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
	        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

		if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_STRING) {

			// set the size of the current string key.
			uint64ToByteArray( arr_key_len , 4, (*output_array) + ptr );

			// copy the string data
			memcpy( (*output_array) + 4 + ptr, arr_key, arr_key_len );

			// move to the next string starting position
			ptr = ptr + 4 + arr_key_len;

			// The data is a double, so convert it
			memcpy( *output_array + ptr, (double*)&Z_DVAL_PP( data ), sizeof( double ) );

			// move to the next position
			ptr = ptr + sizeof( double );
		}
	}
}


/**
 * Converts a PHP hash of Integers ( key=integer, value=integer ) into a Byte Array suitable for HyperDex to store.
 */
void int64Int64HashToByteArray( zval* array, unsigned char** output_array, size_t *bytes )
{
	HashTable*       arr_hash = NULL;
	HashPosition     pointer;
	char*            arr_key;
	unsigned int    arr_key_len;
	unsigned long   index;
	zval**           data     = NULL;

	arr_hash = Z_ARRVAL_P( array );
	int array_count = zend_hash_num_elements( arr_hash );

	// Start by getting the overall size of the memory block that we will need to allocate.
	size_t byteCount = 0;
	for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
	        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
	        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {
		if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_LONG) {
			byteCount = byteCount + 16;
		}
	}

	*bytes = byteCount;

	// Allocate it
	*output_array = (unsigned char*)calloc( 1, byteCount + 1 );

	// Now, fill it with the data that we want.
	size_t ptr = 0;
	for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
	        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
	        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

		if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_LONG) {

			// set the size of the current string key.
			uint64ToByteArray( index , 8, (*output_array) + ptr );

			// move to the next string starting position
			ptr = ptr + 8;

			// The data is an int64, so convert it
			uint64ToByteArray( Z_LVAL_PP(data) , 8, *output_array + ptr );

			// move to the next position
			ptr = ptr + 8;
		}
	}
}


/**
 * Converts a PHP hash of Stings ( key=integer, value=string ) into a Byte Array suitable for HyperDex to store.
 */
void int64StringHashToByteArray( zval* array, unsigned char** output_array, size_t *bytes )
{
	HashTable*       arr_hash = NULL;
	HashPosition     pointer;
	char*            arr_key;
	unsigned int    arr_key_len;
	unsigned long   index;
	zval**           data     = NULL;

	arr_hash = Z_ARRVAL_P( array );
	int array_count = zend_hash_num_elements( arr_hash );

	// Start by getting the overall size of the memory block that we will need to allocate.
	size_t byteCount = 0;
	for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
	        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
	        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {
		if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_LONG) {
			zval temp = **data;
			zval_copy_ctor( &temp );
			convert_to_string( &temp );

			byteCount = byteCount + Z_STRLEN( temp ) + 12;
		}
	}

	*bytes = byteCount;

	// Allocate it
	*output_array = (unsigned char*)calloc( 1, byteCount + 1 );

	// Now, fill it with the data that we want.
	size_t ptr = 0;
	for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
	        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
	        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

		if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_LONG) {

			// set the size of the current string key.
			uint64ToByteArray( index , 8, (*output_array) + ptr );

			// move to the next string starting position
			ptr = ptr + 8;

			// The data is a string, so convert it
			zval temp = **data;
			zval_copy_ctor( &temp );
			convert_to_string( &temp );

			// set the size of the current string element.
			uint64ToByteArray( Z_STRLEN( temp ) , 4, (*output_array) + ptr );

			// copy the string data
			memcpy( (*output_array) + 4 + ptr, Z_STRVAL( temp ), Z_STRLEN( temp ) );

			// move to the next string starting position
			ptr = ptr + 4 + Z_STRLEN( temp );

		}
	}
}


/**
 *  Iterates through a list, and determines if the array is composed entirely of integers or not.
 */
int isArrayAllLong( zval* array )
{
	HashTable*   arr_hash     = NULL;
	HashPosition pointer;
	char*            arr_key;
	unsigned int    arr_key_len;
	unsigned long   index;
	zval**       data         = NULL;
	int          return_value = 1;

	arr_hash = Z_ARRVAL_P( array );
	for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
	        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
	        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

		if( Z_TYPE_PP(data) != IS_LONG ) {
			return_value = 0;
			break;
		}
	}

	return return_value;
}

/**
 *  Iterates through a list, and determines if the array is composed entirely of doubles or not.
 */
int isArrayAllDouble( zval* array )
{
	HashTable*   arr_hash     = NULL;
	HashPosition pointer;
	char*            arr_key;
	unsigned int    arr_key_len;
	unsigned long   index;
	zval**       data         = NULL;
	int          return_value = 1;

	arr_hash = Z_ARRVAL_P( array );
	for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
	        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
	        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

		if( Z_TYPE_PP(data) != IS_DOUBLE ) {
			return_value = 0;
			break;
		}
	}

	return return_value;
}


/**
 *  Iterates through a list, and determines if the array is a hash table (string keys intead of indexes )
 */
int isArrayHashTable( zval* array )
{
	HashTable*       arr_hash     = NULL;
	HashPosition     pointer;
	char*            arr_key;
	unsigned int    arr_key_len;
	unsigned long   index;
	zval**           data         = NULL;
	int              return_value = 0;

	arr_hash = Z_ARRVAL_P( array );
	for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
	        zend_hash_get_current_data_ex( arr_hash, (void**) &data, &pointer ) == SUCCESS;
	        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

		if( zend_hash_get_current_key_ex( arr_hash, &arr_key, &arr_key_len, &index, 0, &pointer ) == HASH_KEY_IS_STRING ) {
			return_value = 1;
			break;
		}
	}

	return return_value;
}


/**
 * Build a HyperDex ATTR structure from a PHP ZVAL structure.
 */
template<class hyperdex_client_attributeT>
void _buildAttrFromZval( zval* data, char* arr_key, hyperdex_client_attributeT* attr, enum hyperdatatype expected_type )
{

	zval temp = *data;
	zval_copy_ctor(&temp);

	attr->attr = arr_key;

	if( Z_TYPE( temp ) == IS_STRING ) {

		//
		// Convert a string
		//
		attr->value = Z_STRVAL(temp);
		attr->value_sz = Z_STRLEN(temp);
		attr->datatype = HYPERDATATYPE_STRING;

	} else if( Z_TYPE( temp ) == IS_LONG ) {

		//
		// Convert a long (integer)
		//
		unsigned char* val = (unsigned char*)calloc( 1, 9 );
		uint64ToByteArray( Z_LVAL( temp ), 8, val );

		attr->value = (const char*)val;
		attr->value_sz = 8;
		attr->datatype = HYPERDATATYPE_INT64;

	} else if( Z_TYPE( temp ) == IS_DOUBLE ) {

		//
		// Convert a Double (floating point number)
		//
		char* val = (char*)calloc( 1, sizeof(double) );
		memcpy( val, (double*)&Z_DVAL( temp ), sizeof(double) );

		attr->value = (const char*)val;
		attr->value_sz = sizeof(double);
		attr->datatype = HYPERDATATYPE_FLOAT;

	} else if( Z_TYPE( temp ) == IS_ARRAY ) {

		//
		// Convert an array into a list (it will either be all integers, all doubles, or all strings)
		//
		unsigned char* val = NULL;
		size_t num_bytes = 0;

		if( 1 == isArrayHashTable( &temp ) ) {
			//
			// If the keys for the array are all alpha-numeric, we have a PHP Hash..
			// We will make that a string/<x> Map for Hyperdex.
			//


			//
			// Check the type in the value of each element.
			// If all are integers, make it a hash of ints.
			// Else, if all are doubles, make it a hash of doubles.
			// Else, by default, convert them to string, and make it a hash of strings.
			//
			if( 1 == isArrayAllLong( &temp ) ) {
				// HashStringInt64
				stringInt64HashToByteArray( &temp, &val, &num_bytes );
				attr->datatype = HYPERDATATYPE_MAP_STRING_INT64;
			} else if( 1 == isArrayAllDouble( &temp ) ) {
				// HashStringFloat
				stringDoubleHashToByteArray( &temp, &val, &num_bytes );
				attr->datatype = HYPERDATATYPE_MAP_STRING_FLOAT;
			} else {
				// HashStringString
				stringStringHashToByteArray( &temp, &val, &num_bytes );
				attr->datatype = HYPERDATATYPE_MAP_STRING_STRING;
			}

			/*	} else if( expected_type > HYPERDATATYPE_MAP_INT64_KEYONLY &&
				            expected_type < HYPERDATATYPE_MAP_FLOAT_KEYONLY ) {
				  //
				  // We have a PHP list, but HyperDex is expecting a Map with numeric keys.
				  //

				  // are the value all integers?
				  if( 1 == isArrayAllLong( &temp ) ) {
			           // HashInt64Int64
			           int64Int64HashToByteArray( &temp, &val, &num_bytes );
				  } else {
			           // HashInt64String
			           int64StringHashToByteArray( &temp, &val, &num_bytes );
				  }
			*/
		}  else {
			//
			// Else, we will have a PHP List and HyperDex wants a list or set, which is OK...
			//

			if( 1 == isArrayAllLong( &temp ) ) {

				//
				// Long Integer array or set
				//

				// A set needs to be sorted...
				int sort = 0;
				if( expected_type == HYPERDATATYPE_SET_INT64 ) {
					sort = 1;
				}

				intListToByteArray( &temp, &val, &num_bytes, sort );

				if( expected_type == HYPERDATATYPE_SET_INT64 ) {
					attr->datatype = HYPERDATATYPE_SET_INT64;
				} else {
					attr->datatype = HYPERDATATYPE_LIST_INT64;
				}

			} else if( 1 == isArrayAllDouble( &temp ) ) {
				//
				// Double precision float array or set
				//

				// A set needs to be sorted...
				int sort = 0;
				if( expected_type == HYPERDATATYPE_SET_FLOAT ) {
					sort = 1;
				}

				doubleListToByteArray( &temp, &val, &num_bytes, sort );

				if( expected_type == HYPERDATATYPE_SET_FLOAT ) {
					attr->datatype = HYPERDATATYPE_SET_FLOAT;
				} else {
					attr->datatype = HYPERDATATYPE_LIST_FLOAT;
				}
			} else {

				//
				// String array or set (also the default if the incoming array is mixed value type)
				//

				// A set needs to be sorted...
				int sort = 0;
				if( expected_type == HYPERDATATYPE_SET_STRING ) {
					sort = 1;
				}

				stringListToByteArray( &temp, &val, &num_bytes, sort );

				if( expected_type == HYPERDATATYPE_SET_STRING ) {
					attr->datatype = HYPERDATATYPE_SET_STRING;
				} else {
					attr->datatype = HYPERDATATYPE_LIST_STRING;
				}

			}
		}
		//
		// Now assign the array character buffer to the value.
		//
		attr->value = (const char*)val;
		attr->value_sz = num_bytes;
	}

}
void buildAttrFromZval( zval* data, char* arr_key, hyperdex_client_attribute* attr, enum hyperdatatype expected_type )
{
	_buildAttrFromZval<hyperdex_client_attribute>(data, arr_key, attr, expected_type );
}

void buildAttrCheckFromZval( zval* data, char* arr_key, hyperdex_client_attribute_check* attr, enum hyperpredicate pred, enum hyperdatatype expected_type)
{
	_buildAttrFromZval<hyperdex_client_attribute_check>(data, arr_key, attr, expected_type );
	attr->predicate=pred;
}


/**
 * Take a list of attributes, and build an Hash Array ZVAL out of it.
 */
void buildArrayFromAttrs( hyperdex_client_attribute* attrs, size_t attrs_sz, zval* data )
{

	array_init(data);

	if( attrs_sz > 0 ) {
		for( int i = 0; i < attrs_sz; ++i ) {

			char* attr_name = estrdup( attrs[i].attr );

			zval *mysubdata;
			ALLOC_INIT_ZVAL(mysubdata);

			buildZvalFromAttr( &attrs[i], mysubdata );
			add_assoc_zval( data, attr_name, mysubdata );
		}
	} else {
		ZVAL_BOOL( data, 0 );
	}
}


/**
 *  Build a ZVAL frmo a specific HyperDex attribute.
 */
void buildZvalFromAttr( hyperdex_client_attribute* attr, zval* data )
{

	if( attr->datatype == HYPERDATATYPE_STRING ) {
		char* r_val = estrndup((char *)attr->value, attr->value_sz);
		ZVAL_STRINGL( data, r_val, attr->value_sz, 0 );
	} else if( attr->datatype == HYPERDATATYPE_INT64 ) {
		ZVAL_LONG( data, byteArrayToUInt64( (unsigned char*)attr->value, attr->value_sz) );
	} else if( attr->datatype == HYPERDATATYPE_FLOAT ) {
		ZVAL_DOUBLE( data, *( (double*) attr->value ) );
	} else if( attr->datatype == HYPERDATATYPE_LIST_STRING ||
	           attr->datatype == HYPERDATATYPE_SET_STRING ) {
		byteArrayToListString( (unsigned char*)attr->value, attr->value_sz, data );
	} else if( attr->datatype == HYPERDATATYPE_LIST_INT64 ||
	           attr->datatype == HYPERDATATYPE_SET_INT64 ) {
		byteArrayToListInt64( (unsigned char*)attr->value, attr->value_sz, data );
	} else if( attr->datatype == HYPERDATATYPE_LIST_FLOAT ||
	           attr->datatype == HYPERDATATYPE_SET_FLOAT ) {
		byteArrayToListFloat( (unsigned char*)attr->value, attr->value_sz, data );
	} else if( attr->datatype == HYPERDATATYPE_MAP_STRING_STRING ) {
		byteArrayToMapStringString( (unsigned char*)attr->value, attr->value_sz, data );
	} else if( attr->datatype == HYPERDATATYPE_MAP_STRING_INT64 ) {
		byteArrayToMapStringInt64( (unsigned char*)attr->value, attr->value_sz, data );
	} else if( attr->datatype == HYPERDATATYPE_MAP_STRING_FLOAT ) {
		byteArrayToMapStringFloat( (unsigned char*)attr->value, attr->value_sz, data );
	} else if( attr->datatype == HYPERDATATYPE_MAP_INT64_STRING ) {
		byteArrayToMapInt64String( (unsigned char*)attr->value, attr->value_sz, data );
	} else if( attr->datatype == HYPERDATATYPE_MAP_INT64_INT64 ) {
		byteArrayToMapInt64Int64( (unsigned char*)attr->value, attr->value_sz, data );
	} else if( attr->datatype == HYPERDATATYPE_MAP_INT64_FLOAT ) {
		byteArrayToMapInt64Float( (unsigned char*)attr->value, attr->value_sz, data );

	} else {
		zend_throw_exception(hyperdex_client_ce_exception, (char*)"Unknown data type", attr->datatype TSRMLS_CC);
	}
}


/**
 * Build a HyperDex Range Query structure from a PHP ZVAL structure.
 */
void buildRangeFromZval( zval* data, char* arr_key, hyperdex_client_attribute_check* fattr, hyperdex_client_attribute_check* sattr)
{

	zval temp = *data;
	zval_copy_ctor(&temp);

	if( Z_TYPE( temp ) == IS_ARRAY ) {
		//
		// Check the type in the value of each element.
		// If all are integers, make it a hash of ints.
		// Else, if all are doubles, make it a hash of doubles.
		// Else, by default, convert them to string, and make it a hash of strings.
		//
		HashTable *arr_hash = Z_ARRVAL(temp);
		HashPosition pointer;
		zval **udata;
		int cntr = 0;
		enum hyperdatatype dtyp;

		for( zend_hash_internal_pointer_reset_ex( arr_hash, &pointer );
		        zend_hash_get_current_data_ex( arr_hash, (void**) &udata, &pointer ) == SUCCESS;
		        zend_hash_move_forward_ex( arr_hash, &pointer ) ) {

			if (cntr==0) {
				dtyp=SimpleBasicType(udata);
				_buildAttrFromZval<hyperdex_client_attribute_check>(*udata, arr_key, fattr, dtyp);
				fattr->predicate=HYPERPREDICATE_GREATER_EQUAL;
			} else if (cntr==1) {
				if (dtyp!=SimpleBasicType(udata)) {
					zend_throw_exception(hyperdex_client_ce_exception, (char*)"Invalid range types", HYPERDEX_CLIENT_WRONGTYPE TSRMLS_CC);
					throw -1;
				}
				_buildAttrFromZval<hyperdex_client_attribute_check>(*udata, arr_key, sattr, dtyp);
				sattr->predicate=HYPERPREDICATE_LESS_EQUAL;
			} else {
				break;
			}
			++cntr;
		}
		if(cntr!=2) {
			zend_throw_exception(hyperdex_client_ce_exception, (char*)"Invalid range values", HYPERDEX_CLIENT_WRONGTYPE TSRMLS_CC);
			throw -1;
		}
	} else {
		// The input *MUST* be an array.
		zend_throw_exception(hyperdex_client_ce_exception, (char*)"Invalid range type", HYPERDEX_CLIENT_WRONGTYPE TSRMLS_CC);
		throw -1;
	}

}

/**
 *  Clean up memory allocated by the system for storing HyperDex values.
 */
template<class hyperdex_client_attributeT>
void _freeAttrVals( hyperdex_client_attributeT *attr, int len )
{
	for( int i = 0; i < len; ++i ) {
		switch( attr[i].datatype ) {
		case HYPERDATATYPE_INT64:
		case HYPERDATATYPE_FLOAT:
		case HYPERDATATYPE_LIST_STRING:
		case HYPERDATATYPE_LIST_INT64:
		case HYPERDATATYPE_SET_STRING:
		case HYPERDATATYPE_SET_INT64:
		case HYPERDATATYPE_MAP_STRING_INT64:
		case HYPERDATATYPE_MAP_STRING_STRING:
		case HYPERDATATYPE_MAP_INT64_STRING:
		case HYPERDATATYPE_MAP_INT64_INT64:
			if( attr[i].value != NULL ) {
				free( (void*)attr[i].value );
				attr[i].value = NULL;
			}
			break;
		}
	}
}
void freeAttrVals( hyperdex_client_attribute *attr, int len )
{
	_freeAttrVals<hyperdex_client_attribute>(attr, len );
}

void freeAttrCheckVals( hyperdex_client_attribute_check *attr, int len )
{
	_freeAttrVals<hyperdex_client_attribute_check>(attr, len );
}

/*
 * Borrowed from PHP iteslf
 */
static int php_array_string_compare(const void *a, const void *b TSRMLS_DC) /* {{{ */
{
	Bucket *f;
	Bucket *s;
	zval result;
	zval *first;
	zval *second;

	f = *((Bucket **) a);
	s = *((Bucket **) b);

	first = *((zval **) f->pData);
	second = *((zval **) s->pData);

	if (string_compare_function(&result, first, second TSRMLS_CC) == FAILURE) {
		return 0;
	}

	if (Z_TYPE(result) == IS_DOUBLE) {
		if (Z_DVAL(result) < 0) {
			return -1;
		} else if (Z_DVAL(result) > 0) {
			return 1;
		} else {
			return 0;
		}
	}

	convert_to_long(&result);

	if (Z_LVAL(result) < 0) {
		return -1;
	} else if (Z_LVAL(result) > 0) {
		return 1;
	}

	return 0;
}
/* }}} */

static int php_array_number_compare(const void *a, const void *b TSRMLS_DC) /* {{{ */
{
	Bucket *f;
	Bucket *s;
	zval result;
	zval *first;
	zval *second;

	f = *((Bucket **) a);
	s = *((Bucket **) b);

	first = *((zval **) f->pData);
	second = *((zval **) s->pData);

	if (numeric_compare_function(&result, first, second TSRMLS_CC) == FAILURE) {
		return 0;
	}

	if (Z_TYPE(result) == IS_DOUBLE) {
		if (Z_DVAL(result) < 0) {
			return -1;
		} else if (Z_DVAL(result) > 0) {
			return 1;
		} else {
			return 0;
		}
	}

	convert_to_long(&result);

	if (Z_LVAL(result) < 0) {
		return -1;
	} else if (Z_LVAL(result) > 0) {
		return 1;
	}

	return 0;
}
/* }}} */


/**
 * Translate HyperDex error / status codes into strings that can be used in exceptions.
 */
char* HyperDexErrorToMsg( hyperdex_client_returncode code )
{

	switch( code ) {
		/* General Statuses */
	case HYPERDEX_CLIENT_SUCCESS:
		return((char*)"Success");
	case HYPERDEX_CLIENT_NOTFOUND:
		return((char*)"Not Found");
	case HYPERDEX_CLIENT_SEARCHDONE:
		return((char*)"Search Done");
	case HYPERDEX_CLIENT_CMPFAIL:
		return((char*)"Compare Failed");
	case HYPERDEX_CLIENT_READONLY:
		return((char*)"Read Only");

		/* Error conditions */
	case HYPERDEX_CLIENT_UNKNOWNSPACE:
		return((char*)"Unknown Space");
	case HYPERDEX_CLIENT_COORDFAIL:
		return((char*)"Coordinator Failure");
	case HYPERDEX_CLIENT_SERVERERROR:
		return((char*)"Server Error");
	case HYPERDEX_CLIENT_POLLFAILED:
		return((char*)"Poll Failed");
	case HYPERDEX_CLIENT_OVERFLOW:
		return((char*)"Overflow");
	case HYPERDEX_CLIENT_RECONFIGURE:
		return((char*)"Reconfigure");
	case HYPERDEX_CLIENT_TIMEOUT:
		return((char*)"Timeout");
	case HYPERDEX_CLIENT_UNKNOWNATTR:
		return((char*)"Unknown Attribute");
	case HYPERDEX_CLIENT_DUPEATTR:
		return((char*)"Duplicate Attribute");
	case HYPERDEX_CLIENT_NONEPENDING:
		return((char*)"None Pending");
	case HYPERDEX_CLIENT_DONTUSEKEY:
		return((char*)"Dont Use Key");
	case HYPERDEX_CLIENT_WRONGTYPE:
		return((char*)"Wrong Attribute Type");
	case HYPERDEX_CLIENT_NOMEM:
		return((char*)"Out of Memory");
	}
}

enum hyperdatatype SimpleBasicType(zval** data) {
	if( Z_TYPE_PP(data) == IS_LONG ) return HYPERDATATYPE_INT64;
	else if( Z_TYPE_PP(data) == IS_DOUBLE ) return HYPERDATATYPE_FLOAT;
	else return HYPERDATATYPE_STRING;
}
