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

sleep(5);

/*
$hdp = new hyperclient('10.207.0.225', 1982);

$val = $hdp->put( 'phonebook','jsmith1', array( 'first' => 'Brad', 'last' => 'Broerman', 'phone' => 123456 ) );

$val = $hdp->get( 'phonebook','jsmith1' );

var_dump( $val );
*/