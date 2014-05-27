--TEST--
HyperdexAdmin tests
--SKIPIF--
<?php if (!extension_loaded("hyperdex")) print "skip"; ?>
--FILE--
<?php

echo "123\n";

?>
===DONE===
--EXPECTF--
123
===DONE===
