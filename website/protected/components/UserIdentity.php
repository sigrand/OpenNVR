<?php

class UserIdentity extends CUserIdentity {
	private $_id;
	public $permissions; // 0 - inactive; 1 - viewer; 2 - operator; 3 - admin; 4 - banned;
	public $isAdmin;
	public $nick;

	public function authenticate() {
		$nick = strtolower($this->username);
		$user = Users::model()->find('LOWER(email)=?', array($nick));
		if($user === null) {
			$this->errorCode = self::ERROR_USERNAME_INVALID;
		} elseif(!$user->validatePassword($this->password, $user->salt)) {
			$this->errorCode = self::ERROR_PASSWORD_INVALID;
		} elseif($user->status == 0) {
			$this->errorCode = self::ERROR_PASSWORD_INVALID;
			return 2;
		} elseif($user->status == 4) {
			// user is banned
			$this->errorCode = self::ERROR_PASSWORD_INVALID;
			return 3;
		} else {
			$this->_id = $user->id;
			$this->username = $user->email;
			$this->setState('isAdmin', $user->status == 3);
			$this->setState('permissions', $user->status);
			$this->setState('nick', $user->nick);
			$this->setState('session_key', md5($user->email.time().uniqid().$user->salt));
			$this->setState('user_ip', Yii::app()->request->userHostAddress);
			Sessions::model()->deleteAllByAttributes(array('user_id' => $user->id));
			$this->errorCode = self::ERROR_NONE;
		}
		return $this->errorCode == self::ERROR_NONE;
	}

	public function getId()	{
		return $this->_id;
	}
}