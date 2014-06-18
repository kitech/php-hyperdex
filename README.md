php-hyperdex
=============
The hyperdex extension provides an API for communicating with the Hyperdex key-value store. 
It is released under the [PHP License, version 3.01](http://www.php.net/license/3_01.txt).

This code was originally developed my at Triton Digital Media, and provided to the open 
source community as a "thank you". We use open source software in our business, and feel that it 
is only right to give back whenever we can.

Ongoing, this code will be maintained by me outside of work hours, and without their involvement.

For more information on Hyperdex, please see http://hyperdex.org/


For comments, or issues, please contact bbroerman@bbroerman.net

In order for the followuing to work you will need to run `sudo apt-get install hyperdex libhyperdex-client-dev libhyperdex-admin-dev libhyperdex-dev` first.

Installing/Configuring
======================

    phpize
    ./configure --enable-hyperdex
    make
    sudo make install

`make install` copies `hyperdex.so` to an appropriate location, but you still need to enable the module in the PHP config file. To do so, either edit your php.ini or add a redis.ini file in `/etc/php5/conf.d` with the following contents: `extension=hyperdex.so`.

You can generate a debian package for PHP5, accessible from Apache 2 by running `./mkdeb-apache2.sh` or with `dpkg-buildpackage` or `svn-buildpackage`.

The extension exports the `Hyperdex\Admin`, `Hyperdex\Client`, `Hyperdex\ClientException` and `Hyperdex\AdminException` classes. 


Usage
====================

The Hyperdex\Client exposes the following methods:

    use Hyperdex\Client;

    bool Client::__construct(string host, int port[, boolean async = false]);
    bool Client::__destruct();

    bool Client::connect(string host, int port[, boolean async = false]);
    bool Client::disconnect();
    
    bool Client::put(string space, string key, array attributes);
    bool Client::putAttr(string space, string key, string attr_name, Mixed value);
    bool Client::condPut(string space, string key, array conditionals, array attributes);
    
    bool Client::lPush(string space, string key, array attributes);
    bool Client::rPush(string space, string key, array attributes);
    
    bool Client::setAdd(string space, string key, array attributes);
    bool Client::setRemove(string space, string key, array attributes);
    bool Client::setUnion(string space, string key, array attributes);
    bool Client::setIntersect(string space, string key, array attributes);
    
    bool Client::add(string space, string key, array attributes);
    bool Client::sub(string space, string key, array attributes);
    bool Client::mul(string space, string key, array attributes);
    bool Client::div(string space, string key, array attributes);
    bool Client::mod(string space, string key, array attributes);
    bool Client::and(string space, string key, array attributes);
    bool Client::or(string space, string key, array attributes);
    bool Client::xor(string space, string key, array attributes);
    
    array Client::search(string space, array cond_attrs, array range_attrs);
    
    array Client::get(string space, string key);
    mixed Client::getAttr(string space, string key, string attr_name);
    
    bool Client::del(string space, string key);
    
    string Client::errorMessage();
    
    bool Client::loop();
    

The Hyperdex\Admin exposes the following methods:

    use Hyperdex\Admin;

    bool Admin::__construct(string host, int port[, boolean async = false]);
    bool Admin::__destruct();
    
    string Admin::dumpConfig();
    bool Admin::readOnly(int ro);
    bool Admin::waitUntilStable();
    bool Admin::faultTolerance(string space, int ft);
    
    bool Admin::validateSpace(string descript);
    bool Admin::addSpace(string descript);
    bool Admin::rmSpace(string space);
    array Admin::listSpaces();
    
    bool Admin::serverRegister(string/Uint64 token, string host);
    bool Admin::serverOnline(string/Uint64 token);
    bool Admin::serverOffline(string/Uint64 token);
    bool Admin::serverForget(string/Uint64 token);
    bool Admin::serverKill(string/Uint64 token);
    
    string Admin::errorMessage();
    int    Admin::errorCode();
    
    bool Admin::loop();
    

Example:
=========

    use Hyperdex\Client;
    $hdp = new Client('127.0.0.1', 1982);
    $val = $hdp->put('phonebook','jsmith1', array('first' => 'Brad', 'last' => 'Broerman', 'phone' => 123456));
    $val = $hdp->get('phonebook','jsmith1');
    var_dump($val);
