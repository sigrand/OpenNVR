<?php

class UsersForm extends CFormModel {
	private $data = array();
	
	public function __get($key) {
		return (isset($this->data[$key]) ? $this->data[$key] : null);
	}
	
	public function __set($key, $value) {
		$this->data[$key] = $value;
	}
}