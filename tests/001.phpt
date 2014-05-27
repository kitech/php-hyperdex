--TEST--
HyperdexAdmin Host not exist tests
--SKIPIF--
<?php if (!extension_loaded("hyperdex")) print "skip"; ?>
--FILE--
<?php

$adm = new HyperdexAdmin('10.207.0.222', 1982); // ip not exists or port not open
var_dump($adm);

$no_arg_methods = array('dump_config', 'list_spaces', 'wait_until_stable', 'loop');
foreach ($no_arg_methods as $idx => $m) {
    $ret = $adm->$m();
    $emsg = $adm->error_message();
    echo $m . ",$emsg\n";
    var_dump($ret);
}

$str_arg_methods = array('add_space', 'rm_space', 'validate_space');
foreach ($str_arg_methods as $idx => $m) {
    $ret = $adm->$m('hahaha');
    $emsg = $adm->error_message();
    echo $m . ",$emsg\n";
    var_dump($ret);
}


?>
===DONE===
--EXPECTF--
object(HyperdexAdmin)#1 (0) {
}
dump_config,no outstanding operations to process
bool(false)
list_spaces,no outstanding operations to process
bool(false)
wait_until_stable,no outstanding operations to process
bool(false)
loop,no outstanding operations to process
bool(false)
add_space,no outstanding operations to process
bool(false)
rm_space,no outstanding operations to process
bool(false)
validate_space,bad space syntax error at line 1 column 1
bool(false)
===DONE===
