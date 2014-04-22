<?php

class UserForm extends CFormModel {
	public $nick;
	public $email;
	public $pass;
	public $rang = 1;

	public function rules()	{
		return array(
			array('pass, nick, email', 'required'),
			array('email', 'unique', 'attributeName' => 'email', 'className' => 'Users', 'message' => Yii::t('errors', 'An account with this email address already exists')),
			array('nick', 'unique', 'attributeName' => 'nick', 'className' => 'Users', 'message' => Yii::t('errors', 'An account with this nick already exists')),
			array('email', 'email', 'message' => Yii::t('errors', 'Wrong email format')),
			array('rang', 'safe'),
			);
	}

	public function register() {
		$user = new Users;
		$user->attributes = $this->attributes;
		$salt = md5(uniqid().time());
		$user->email = $this->email;
		$user->salt = $salt;
		if(Yii::app()->user->permissions == 3) {
			$user->status = $this->rang;
		} else {
			$user->status = 1;
		}
		$user->pass = crypt(trim($this->pass).$salt);
		if($user->validate() && $user->save()) {
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