<?php

class ProfileForm extends CFormModel {
	public $old_pass;
	public $new_pass;
	public $time_offset;
	public $user;
	public $options;
	private $type;

	function __construct() {
		$this->user = Users::model()->findByPk(Yii::app()->user->getId());
		$this->time_offset = $this->user->time_offset;
		$this->options = $this->user->options;
	}

	public function setType($type) {
		$this->type = $type;
	}

	public function rules()	{
		if($this->type == 'pass') {
			return array(
				array('old_pass, new_pass', 'required', 'message' => Yii::t('errors', 'Can\'t be empty')),
				array('old_pass', 'checkpass'),
				);
		} else {
			return array(
				array('time_offset', 'required', 'message' => Yii::t('errors', 'Can\'t be empty')),
				array('time_offset', 'numerical'),
				);
		}
	}

	public function change() {
		if($this->type == 'pass') {
			$this->user->pass = crypt(trim($this->new_pass).$this->user->salt);
			return $this->user->save();
		} else {
			$this->user->time_offset = $this->time_offset;
			return $this->user->save();
		}
	}

	public function attributeLabels() {
		return array(
			'time_offset' => Yii::t('user', 'Timezone'),
			'new_pass' => Yii::t('user', 'New password'),
			'old_pass' => Yii::t('user', 'Old password'),
			'options' => Yii::t('user', 'Show archive by time:'),
			);
	}

	public function checkpass($attribute, $params) {
		if((!empty($this->old_pass) || !empty($this->new_pass)) && !$this->user->validatePassword($this->old_pass, $this->user->salt)) {
			$this->addError($attribute, Yii::t('errors', 'wrong password'));
		}
	}
}