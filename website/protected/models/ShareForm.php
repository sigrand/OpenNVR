<?php

class ShareForm extends CFormModel {
	private $camBuff;
	private $emailBuff;
	public $hcams;
	public $cams;
	public $emails;

	public function rules()	{
		return array(
			array('cams, emails, hcams', 'required', 'message' => 'Не может быть пустым'),
			array('hcams', 'isOwned'),
			);
	}

	public function beforeValidate() {
		if(parent::beforeValidate()) {
			$cams = array_map('trim', explode(',', $this->hcams));
			$c = count($cams);
			$this->camBuff = Cams::model()->findAllByAttributes(array('id' => $cams));
			if(empty($this->camBuff) || count($this->camBuff) != $c) {
				$this->addError('cams', $c > 1 ? 'Одна из камер не сушествует' : 'Камера не существует');
				return false;
			}
			$emails = array_map('trim', explode(',', $this->emails));
			$c = count($emails);
			$this->emailBuff = Users::model()->findAllByAttributes(array('email' => $emails));
			if(empty($this->emailBuff) || count($this->emailBuff) != $c) {
				$this->addError('emails', $c > 1 ? 'Один из пользователей не сушествует' : 'Пользователь не существует');
				return false;	
			}
			return true;
		}
		return false;
	}

	public function isOwned() {
		if($this->camBuff) {
			$c = count($this->camBuff);
			foreach ($this->camBuff as $cam) {
				if($cam->user_id != Yii::app()->user->getId()) { $this->addError('cams', $c > 1 ? 'Камеры вам не принадлежат.' : 'Камера вам не принадлежит'); return false; }
			}
			return true;
		}
		return false;
	}

	public function save() {
		$id = Yii::app()->user->getId();
		foreach($this->camBuff as $cam) {
			foreach($this->emailBuff as $user) {
				$n = new Notify;
				$n->note('Вам расшарена камера '.$cam->name, $id, $user->id);
				$Shared = Shared::model()->findByAttributes(array('owner_id' => $id, 'user_id' => $user->id, 'cam_id' => $cam->id, 'public' => 0));
				if(!$Shared) {
					$Shared = new Shared;
					$Shared->owner_id = $id;
					$Shared->user_id = $user->id;
					$Shared->cam_id = $cam->id;
					$Shared->save();
				}
			}
		}
		return true;
	}

	public function attributeLabels() {
		return array(
			'hcams' => 'Камеры, можно неск. разделять ","',
			'cams' => 'Камеры, можно неск. разделять ","',
			'emails' => 'Email`ы/Группы, можно неск. разделять ","'
			);
	}

}