<?php

class installer {
	private $results;
	private $status;
	private $steps = array();
	private $checkers = array(
		'e' => 'exists',
		'em' => 'isEmpty',
		'w' => 'writeable',
		'r' => 'readeable',
		'f' => 'filled',
		'h' => 'hash',
		'c' => 'connection',
		'd' => 'db',
		's' => 'steps',
		);

	public function check($modes, $object) {
		$this->free('check');
		$this->caller = 'check';
		if(!is_array($object)) { $object = array($object); }
		foreach($object as $o) {
			$this->set('object', $o);
			foreach (explode('|', $modes) as $mode) {
				if(isset($this->checkers[$mode])) {
					$function = $this->checkers[$mode];
					if(!$this->$function($o)) {
						return false;
					}
				}
			}
		}
		return true;
	}

	public function free($what) {
		$this->results[$what] = array();
	}

	public function step($step, $result) {
		$this->steps[$step] = (int)$result;
	}

	public function get($what) {
		if(strstr($what, '_')){
			$caller = explode('_', $what);
			$what = $caller[0];
		}
		return $this->results[$what];
	}

	public function addResult($function, $result) {
		$this->set($function, $result);
	}

	public function set($what, $value) {
		if($this->caller) {
			if(isset($this->results[$this->caller][$what])) {
				if(!is_array($this->results[$this->caller][$what])) {
					$this->results[$this->caller][$what] = array($this->results[$this->caller][$what]);
				}
				$this->results[$this->caller][$what][] = $value;
			} else {
				$this->results[$this->caller][$what] = $value;
			}
		} else {
			$this->results[$what] = $value;
		}
	}

	private function exists($object) {
		$result = (int)file_exists($object);
		$this->addResult('exists', $result);
		return $result;
	}

	private function writeable($object) {
		$result = (int)is_writeable($object);
		$this->addResult('writeable', $result);
		return $result;
	}

	private function filled($object) {
		$object = file_get_contents($object);
		$result = (int)!empty($object);
		$this->addResult('filled', $result);
		return $result;
	}

	private function isEmpty($object) {
		$result = (int)!empty($object);
		$this->addResult('isEmpty', $result);
		return $result;
	}

	private function readeable($object) {
		$result = (int)is_readable($object);
		$this->addResult('readable', $result);
		return $result;
	}

	private function hash($object) {
		$object = explode(':', $object);
		$result = (int)($object[1] == md5_file($object[0]));
		$this->addResult('hash', $result);
		return $result;
	}

	private function steps($object) {
		if($object) {
			$tmp = $this->steps['db_exists'];
			$this->steps['db_exists'] = 1;
		}
		$this->status = $result = array_reduce($this->steps, function ($e, $i) { $e *= $i; return $e*$i; }, 1);
		$this->steps['db_exists'] = isset($tmp) ? $tmp : $this->steps['db_exists'];
		$this->addResult('steps', $result);
		return $result;
	}

	private function connection($object) {
		$object = json_decode($object, 1);
		try {
			$this->pdo = new PDO('mysql:host='.$object['{h}'].';', $object['{u}'], $object['{p}']);
			$this->pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_SILENT);
		} catch (PDOException $e) {
			$this->addResult('connection', 0);
			return 0;
		}
		$this->addResult('connection', 1);
		return 1;
	}	

	private function db($object) {
		$object = json_decode($object, 1);
		if(!$this->pdo) { $this->addResult('database', 0); return 0; }
		try {
			if($this->pdo->exec('USE '.$object['{d}']) !== false) {
				$this->addResult('database', 0);
				return 0;
			}
		} catch (PDOException $e) {
			$this->addResult('database', 0);
			return 0;
		}
		$this->addResult('database', 1);
		return 1;
	}

	public function __destruct() {
		$this->pdo = null;
	}

	public function makeConfig($path, $config) {
		$path = $path.'/protected/config/main.php';
		$result = file_put_contents($path, str_replace(array_keys($config), $config, file_get_contents($path)));
		return (bool)$result;
	}

	public function makeDB($file, $db, $use_current = false) {
		if($use_current && !$this->steps['db_exists']) { return 2; }
		if($this->pdo) {
			if($this->pdo->exec('CREATE DATABASE IF NOT EXISTS `'.$db['{d}'].'`;USE `'.$db['{d}'].'`;')) {
				$result = !(bool)$this->pdo->exec(file_get_contents($file)) && !(bool)(int)$this->pdo->errorCode();
				return $result;
			}
		}
	}

	public function getStatus() {
		if(!isset($this->status)) { return '{instruction}'; }
		return $this->status ? '{install_success}' : '{fail}';
	}

}

?>