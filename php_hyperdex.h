/*
  +----------------------------------------------------------------------+
  | HyperDex client plugin                                               |
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

/* $Id$ */

#ifndef PHP_HYPERDEX_H
#define PHP_HYPERDEX_H

extern zend_module_entry hyperdex_module_entry;
#define phpext_hyperdex_ptr &hyperdex_module_entry

#ifdef PHP_WIN32
#	define PHP_HYPERDEX_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_HYPERDEX_API __attribute__ ((visibility("default")))
#else
#	define PHP_HYPERDEX_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

PHP_MINIT_FUNCTION(hyperdex);
PHP_MSHUTDOWN_FUNCTION(hyperdex);
PHP_RINIT_FUNCTION(hyperdex);
PHP_RSHUTDOWN_FUNCTION(hyperdex);
PHP_MINFO_FUNCTION(hyperdex);

/* client method */
PHP_METHOD(HyperdexClient, __construct);
PHP_METHOD(HyperdexClient, __destruct);
PHP_METHOD(HyperdexClient, connect);
PHP_METHOD(HyperdexClient, disconnect);

PHP_METHOD(HyperdexClient, put);
PHP_METHOD(HyperdexClient, putAttr);
PHP_METHOD(HyperdexClient, condPut);

PHP_METHOD(HyperdexClient, lPush);
PHP_METHOD(HyperdexClient, rPush);

PHP_METHOD(HyperdexClient, setAdd);
PHP_METHOD(HyperdexClient, setRemove);
PHP_METHOD(HyperdexClient, setUnion);
PHP_METHOD(HyperdexClient, setIntersect);

PHP_METHOD(HyperdexClient, add);
PHP_METHOD(HyperdexClient, sub);
PHP_METHOD(HyperdexClient, mul);
PHP_METHOD(HyperdexClient, div);
PHP_METHOD(HyperdexClient, mod);
PHP_METHOD(HyperdexClient, and);
PHP_METHOD(HyperdexClient, or);
PHP_METHOD(HyperdexClient, xor);
    
PHP_METHOD(HyperdexClient, search);
PHP_METHOD(HyperdexClient, get);
PHP_METHOD(HyperdexClient, getAttr);

PHP_METHOD(HyperdexClient, del);
PHP_METHOD(HyperdexClient, loop);
PHP_METHOD(HyperdexClient, errorMessage);
PHP_METHOD(HyperdexClient, errorLocation);


/* admin method */
PHP_METHOD(HyperdexAdmin, __construct);
PHP_METHOD(HyperdexAdmin, __destruct);
PHP_METHOD(HyperdexAdmin, dumpConfig);
PHP_METHOD(HyperdexAdmin, readOnly);
PHP_METHOD(HyperdexAdmin, waitUntilStable);
PHP_METHOD(HyperdexAdmin, faultTolerance);
PHP_METHOD(HyperdexAdmin, validateSpace);
PHP_METHOD(HyperdexAdmin, addSpace);
PHP_METHOD(HyperdexAdmin, rmSpace);
PHP_METHOD(HyperdexAdmin, listSpaces);
PHP_METHOD(HyperdexAdmin, serverRegister);
PHP_METHOD(HyperdexAdmin, serverOnline);
PHP_METHOD(HyperdexAdmin, serverOffline);
PHP_METHOD(HyperdexAdmin, serverForget);
PHP_METHOD(HyperdexAdmin, serverKill);
PHP_METHOD(HyperdexAdmin, loop);
PHP_METHOD(HyperdexAdmin, errorMessage);
PHP_METHOD(HyperdexAdmin, errorCode);
PHP_METHOD(HyperdexAdmin, errorLocation);

/* 
  	Declare any global variables you may need between the BEGIN
	and END macros here:     
*/
ZEND_BEGIN_MODULE_GLOBALS(hyperdex)  
ZEND_END_MODULE_GLOBALS(hyperdex)

/* In every utility function you add that needs to use variables 
   in php_hyperdex_globals, call TSRMLS_FETCH(); after declaring other 
   variables used by that function, or better yet, pass in TSRMLS_CC
   after the last function argument and declare your utility function
   with TSRMLS_DC after the last declared argument.  Always refer to
   the globals in your function as HYPERDEX_G(variable).  You are 
   encouraged to rename these macros something shorter, see
   examples in any other php module directory.
*/

#ifdef ZTS
#define HYPERDEX_G(v) TSRMG(hyperdex_globals_id, zend_hyperdex_globals *, v)
#else
#define HYPERDEX_G(v) (hyperdex_globals.v)
#endif

#endif	/* PHP_HYPERDEX_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
