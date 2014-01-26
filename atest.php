<?php

function config_parser($config)
{
    $hcfg = array();
    
    $cluster_exp = '/^cluster (\d+)$/';
    $verion_exp = '/^version (\d+)$/';
    $flag_exp = '/^flag (\d+)$/';
    $server_exp = '/^server (\d+) ([0-9.:]+) (\w+)$/';
    $space_exp = '/^space (\d+) (\w+)$/';
    $fault_tolerance_exp = '/fault_tolerance (\d+)$/';
    $predecessor_width_exp = '/predecessor_width (\d+)$/';
    $attribute_exp = '/attribute (\w+) (\w+)$/';
    $subspace_exp = '/subspace (\d+)$/';
    $subspace_attributes_exp = '/attributes (\w+)$/';
    $region_exp = '/region (\d+) lower/';

    $parser_mod = 'global'; // space

    $space = '';
    $subspace = '';
    $ssno = '';

    $lines = explode("\n", $config);
    foreach ($lines as $lno => $lstr) {
        if (preg_match($server_exp, $lstr, $mat)) {
            $hcfg['servers'][$mat[1]] = array('token' => $mat[1], 'host' => $mat[2], 'status' => $mat[3]);
            continue;
        }

        if (preg_match($space_exp, $lstr, $mat)) {
            // print_r($mat);
            $space = $mat[2];
            $hcfg[$space] = array('name' => $space, 'sno' => $mat[1]);
            continue;
        }
        
        if (preg_match($fault_tolerance_exp, $lstr, $mat)) {
            $hcfg[$space]['fault_tolerance'] = $mat[1];
            continue;
        }

        if (preg_match($predecessor_width_exp, $lstr, $mat)) {
            $hcfg[$space]['predecessor_width'] = $mat[1];
            continue;
        }

        if (preg_match($attribute_exp, $lstr, $mat)) {
            $hcfg[$space]['attribute'][$mat[1]] = array('name' => $mat[1], 'type' => $mat[2]);
            continue;
        }

        if (preg_match($subspace_exp, $lstr, $mat)) {
            $ssno = $mat[1];
            $hcfg[$space]['subspace']['attributes'][$ssno] = array('ssno' => $mat[1]);
            continue;
        }

        if (preg_match($subspace_attributes_exp, $lstr, $mat)) {
            $hcfg[$space]['subspace']['attributes'][$ssno]['attributes'] = $mat[1];
            $hcfg[$space]['subspace']['region_count'] = 0;
            continue;
        }

        if (preg_match($region_exp, $lstr, $mat)) {
            $hcfg[$space]['subspace']['region_count'] += 1;
            continue;
        }
    }

    return $hcfg;
}


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
var_dump($ret,'------');
if ($ret != false) {
    $hcfg = config_parser($ret);
    print_r($hcfg);
}

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


// 这个token是可以指定的，如果不指定的情况下，使用服务器自己随机生成的token。
// 正确的用法，使用dump_config找到某个结点的token，在此使用这几个函数对其操作。
$token = "16418458729099063566";
var_dump($token);
// $token = 122222222223;
//*
$ret = $adm->server_register($token, '10.207.0.226:2013');
var_dump("server_register:", $ret, $adm->error_message());
//*/
/*
$ret = $adm->server_online($token);
var_dump("server_online:", $ret, $adm->error_message());
*/
//*
$ret = $adm->server_offline($token);
var_dump("server_offline:", $ret, $adm->error_message());
//*/
/*
$ret = $adm->server_forget($token);
var_dump("server_forget:", $ret, $adm->error_message());
*/
/*
$ret = $adm->server_kill($token);
var_dump("server_kill:", $ret, $adm->error_message());
*/

echo "done.......\n";
sleep(5);

/*
$hdp = new hyperclient('10.207.0.225', 1982);

$val = $hdp->put( 'phonebook','jsmith1', array( 'first' => 'Brad', 'last' => 'Broerman', 'phone' => 123456 ) );

$val = $hdp->get( 'phonebook','jsmith1' );

var_dump( $val );
*/