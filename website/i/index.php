<?php
ini_set('error_reporting', E_ALL);
ini_set('display_errors', 'On');
ini_set('display_startup_errors', 'On');
set_time_limit(1000);
define('DIR', dirname(__FILE__).'/');
include DIR.'langs/ru.php';
include DIR.'lib/installer.php';
include DIR.'lib/dropWebLight.php';
include DIR.'lib/http.php';

$i = new installer;
$v = '0.1';
$repo = 'https://www.dropbox.com/sh/5heu2fsn394fnka/QTmCe0-vy7';
$h = new http;
$h->setProxy('localhost', 666);
$d = new dropWebLight($repo, $h);

$file_list = 'file_list.json';
$c['{collapsible}'] = 'false';
$use_current = isset($_POST['use_current']) && $_POST['use_current'] == 'on';
$lang['{use_this}'] = '<br/><input type="checkbox" name="use_current" class="use" '.($use_current ? 'checked' : '').'> Использовать текущую?';
$c['{install_path}'] = DIR;

if(!$i->check('e|w', DIR.'tmp')) {
	mkdir(DIR.'tmp');
}

if(!isset($_POST['app_name']) || !isset($_POST['install_path'])) {
	$c['{app}'] = $lang['{app}'];
	$c['{install_path}'] = DIR.'tmp/';
} else {
	$c['{app}'] = $_POST['app_name'];
	$c['{install_path}'] = $_POST['install_path'];
}

$i->check('em', $c['{app}']) ? $i->step('app_name', true) : $i->step('app_name', false);
$c = listResult('options', $i, $c);
$i->check('e|w', $c['{install_path}']) ? $i->step('install_path', true) : $i->step('install_path', false);
$c = listResult('perms', $i, $c);

if(!$i->check('e|r|f', DIR.$file_list)) {
	$d->get($file_list, DIR);
}
if($i->check('e|r|f', DIR.$file_list)) {
	$i->step('dist', true);
	$files = json_decode(file_get_contents(DIR.$file_list), 1);
	if(!$i->check('e|r|f', $files['files'])) { //TODO: add button download files
		$d->get($files['files'], DIR);
		$i->check('e|r|f', $files['files']);
	}
} else {
	$i->step('dist', false);
}
$c = listResult('files', $i, $c);


$db['name'] = '{database}';

if(!isset($_POST['hostname'])) {
	$db['{h}'] = $c['{c_hostname}'] = 'localhost';
	$db['{u}'] = $c['{c_user}'] = 'root';
	$db['{p}'] = $c['{c_pass}'] = '';
	$db['{d}'] = $c['{c_database}'] = 'web_cams';
} else {
	$db['{h}'] = $c['{c_hostname}'] = $_POST['hostname'];
	$db['{u}'] = $c['{c_user}'] = $_POST['user'];
	$db['{p}'] = $c['{c_pass}'] = $_POST['pass'];
	$db['{d}'] = $c['{c_database}'] = $_POST['database'];
}

$d = json_encode($db);
$i->check('c', $d) ? $i->step('db_conn', true) : $i->step('db_conn', false);
$c = listResult('db', $i, $c);
$i->check('d', $d) ? $i->step('db_exists', true) : $i->step('db_exists', false);
$c = listResult('db', $i, $c);

if(isset($_POST['install']) && ($c['{collapsible}'] = 'true') /* gcode */ && $i->check('s', $use_current)) {
	$zip = new ZipArchive;
	foreach($files as $file) {
		if(!strstr($file, '.zip')) { 
			if(strstr($file, '.sql')) {
				$dbf = $file;
			}
			continue;
		}
		$res = $zip->open(DIR.$file);
		if($res) {
			$zip->extractTo(DIR.'tmp/');
			$zip->close();	
		}
	}
	unset($db['name']);
	$config = $db + array('{app}' => $c['{app}']);
	$i->step('make_db', $i->makeDB($dbf, $db, $use_current));
	$i->step('make_config', $i->makeConfig($c['{install_path}'], $config));
	if(strstr($c['{install_path}'], DIR)) {
		$url = $_SERVER['HTTP_HOST'].$_SERVER['REQUEST_URI'].str_replace(DIR, '', $c['{install_path}']);
		$path = '<a href="//'.$url.'" target="_blank">'.$url.'</a>';
	} else {
		$path = $c['{install_path}'];
	}
	$lang['{install_success}'] = str_replace('{install_path}', $path, $lang['{install_success}']);
	$c['{collapsible}'] = 'true';
}

$c['{install_result}'] = $i->getStatus();

if($c['{install_result}'] == '{install_success}') { // temp. dirty hack
	$c['{db_status}'] = 'success';
	$c['{db_info}'] = '{success}';
}

$lang = $c + $lang;

echo str_replace(array_keys($lang), $lang, file_get_contents(DIR.'tpl.html'));

function result($section, $status, $result) {
	$c['{'.$section.'_check_result}'] = $result;
	$c['{'.$section.'_status}'] = $status;
	$c['{'.$section.'_info}'] = '{'.$status.'}';
	return $c;
}

function listResult($section, $i, $c = '') {
	$c['{'.$section.'_check_result}'] = !isset($c['{'.$section.'_check_result}']) ? '' : $c['{'.$section.'_check_result}'];
	$status = 1;
	$results = $i->get('check_result');
	$results['object'] = isset($results['object'][0]) && $results['object'][0] == '{' && $results['object'][strlen($results['object'])-1] == '}' ? json_decode($results['object'], 1) : $results['object'];
	foreach($results as $key => $result) {
		if($key == 'object') { continue; }
		if(is_array($results['object']) && !isset($results['object']['name'])) {
			foreach($results['object'] as $k => $o) {
				if($results[$key][$k] == 1) {
					$c['{'.$section.'_check_result}'] .= $o.' {is_'.$key.'}<br/>';
				} else {
					$c['{'.$section.'_check_result}'] .= $o.' {is_not_'.$key.'}<br/>';
					$status = 0;
				}	
			}
		} else {
			if($result == 1) {
				$c['{'.$section.'_check_result}'] .= (is_array($results['object']) ? $results['object']['name'] : $results['object']).' {is_'.$key.'}<br/>';
			} else {
				$c['{'.$section.'_check_result}'] .= (is_array($results['object']) ? $results['object']['name'] : $results['object']).' {is_not_'.$key.'}<br/>';
				$status = 0;
			}
		}
	}
	$status = $status ? 'success' : 'warning';
	$c['{'.$section.'_status}'] = $status;
	$c['{'.$section.'_info}'] = '{'.$status.'}';
	return $c;
}

?>