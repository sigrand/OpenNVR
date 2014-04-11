<?php

class dropWebLight {
	private $repo = '';
	private $http;
	private $files = array();

	public function __construct($repo, $http) {
		$this->http = $http;
		$this->repo = $repo;
		$html = $this->http->get($this->repo);
		preg_match_all('/<div class="filename-col"><a href="(.*?)"/si', $html, $f);
		$this->files = array_combine(array_map(array($this, 'extractFiles'), $f[1]), $f[1]);
	}

	public function getFileList() {
		return $this->files;
	}

	public function get($files, $dir) {
		if(is_array($files)) {
			$s = 1;
			foreach ($files as $file) {
				$s *= file_put_contents($dir.$file, $this->http->get($this->files[$file].'?dl=1'));
			}
			return $s;
		} else {
			if($this->files[$files]) {
				return file_put_contents($dir.$files, $this->http->get($this->files[$files].'?dl=1'));
			}
		}
	}

	private function extractFiles($file) {
		$parts = explode('/', $file);
		return $parts[count($parts)-1];
	}
}
?>