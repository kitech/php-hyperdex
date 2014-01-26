<?php

$adm = new HyperdexAdmin('10.207.0.225', 1982);
var_dump($adm);
$ret = $adm->add_space('haha');
var_dump($ret);
$return_value = array();
$ret = $adm->list_spaces();
var_dump($ret);

$ret = $adm->rm_space('hahaaaa');
var_dump($ret);

$ret = $adm->error_message();
var_dump($ret);

$ret = $adm->error_location();
var_dump($ret);

$ret = $adm->dump_config();
// var_dump($ret,'------');

$ret = $adm->read_only(1);
var_dump($ret);

$ret = $adm->read_only(0);
var_dump($ret);

// $ret = $adm->wait_until_stable();
// var_dump($ret);



$space_desc = "space datasettest\nkey fkey\nattributes\nstring strval,\nint64 intval,\nfloat dblval,\nlist(string) liststrval,\nlist(int64) listintval,\nlist(float) listdblval,\nset(string) setstrval,\nset(int64) setintval,\nset(float) setdblval,\nmap(string,string) mapstrval,\nmap(string,int64) mapintval,\nmap(string,float) mapdblval\n";
$ret = $adm->validate_space($space_desc);
var_dump("validate_space:", $ret, $adm->error_message());



$ret = $adm->add_space($space_desc);
var_dump("add space:", $ret, $adm->error_message());



$ret = $adm->server_register(12222222222, '10.207.0.226:1982');
var_dump("server_register:", $ret, $adm->error_message());

$ret = $adm->server_online(12222222222);
var_dump("server_online:", $ret, $adm->error_message());

$ret = $adm->server_offline(12222222222);
var_dump("server_offline:", $ret, $adm->error_message());

$ret = $adm->server_forget(12222222222);
var_dump("server_forget:", $ret, $adm->error_message());

$ret = $adm->server_kill(12222222222);
var_dump("server_kill:", $ret, $adm->error_message());

echo "done.......\n";
sleep(5);

/*
$hdp = new hyperclient('10.207.0.225', 1982);

$val = $hdp->put( 'phonebook','jsmith1', array( 'first' => 'Brad', 'last' => 'Broerman', 'phone' => 123456 ) );

$val = $hdp->get( 'phonebook','jsmith1' );

var_dump( $val );
*/