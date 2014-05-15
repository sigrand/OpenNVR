<?php
define('DIR', dirname(__FILE__).'/');
define('TMP', DIR.'/tmp/');
define('LIB', DIR.'/lib/');
define('BKP', DIR.'/backups/');
include LIB.'http.php';
include LIB.'dropWebLight.php';
include LIB.'driversManager.php';
include LIB.'updateHelper.php';
include LIB.'backupHelper.php';
?>