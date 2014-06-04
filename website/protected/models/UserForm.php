<?php

class UserForm extends CFormModel {
	public $id;
	public $nick;
	public $email;
	public $pass;
	public $rang = 1;
	public $server_id = 0;
	public $quota = 0;
	public $isNewRecord = 1;

	public function rules()	{
		return array(
			array('pass, nick, email', 'required'),
			array('email', 'unique', 'attributeName' => 'email', 'className' => 'Users', 'message' => Yii::t('errors', 'An account with this email address already exists'), 'on'=>'insert'),
			array('nick', 'unique', 'attributeName' => 'nick', 'className' => 'Users', 'message' => Yii::t('errors', 'An account with this nick already exists'), 'on'=>'insert'),
			array('email', 'email', 'message' => Yii::t('errors', 'Wrong email format')),
			array('rang, server_id, quota, id', 'safe'),
			);
	}

	public function init() {
		$this->quota = $this->quota ? $this->quota : Settings::model()->getValue('default_quota');
	}


	public function load($id) {
		$user = Users::model()->findByPK($id);
		$this->attributes = $user->attributes;
		$this->rang = $user->status;
		$this->isNewRecord = 0;
	}

	public function save() {
		$user = Users::model()->findByPK($this->id);
		$user->attributes = $this->attributes;
		//$salt = md5(uniqid().time());
		//$user->email = $this->email;
		//$user->salt = $salt;
		//$user->server_id = $this->server_id;
		if(Yii::app()->user->permissions == 3) {
			$user->status = $this->rang;
		} else {
			$user->status = $user->status;
		}
		//$user->pass = crypt(trim($this->pass).$salt);
		if($user->validate() && $user->save()) {	
			if($user->status >= 2 && $this->server_id != 0) {
				Yii::import('ext.moment.index', 1);
				$momentManager = new momentManager($this->server_id);
				$momentManager->addQuota($user->id, $this->quota);
			}
			return 1;
		}
	}

	public function register() {
		$user = new Users;
		$user->attributes = $this->attributes;
		$salt = md5(uniqid().time());
		$user->email = $this->email;
		$user->salt = $salt;
		$user->server_id = $this->server_id;
		if(Yii::app()->user->permissions == 3) {
			$user->status = $this->rang;
		} else {
			$user->status = 1;
		}
		$user->pass = crypt(trim($this->pass).$salt);
		if($user->validate() && $user->save()) {	
			if($user->status >= 2 && $this->server_id != 0) {
				Yii::import('ext.moment.index', 1);
				$momentManager = new momentManager($this->server_id);
				$momentManager->addQuota($user->id, $this->quota);
			}
			return 1;
		}
	}

	public function attributeLabels() {
		return array(
			'pass' => Yii::t('admin', 'Password'),
			'nick' => Yii::t('admin', 'Nick'),
			'rang' => Yii::t('admin', 'Rang')
			);
	}
}