<?php

/**
 * This is the model class for table "cams".
 *
 * The followings are the available columns in table 'cams':
 * @property integer $id
 * @property integer $user_id
 * @property integer $server_id
 * @property string $name
 * @property string $desc
 * @property string $url
 * @property string $prev_url
 * @property integer $show
 * @property integer $public
 * @property string $time_offset
 * @property string $coordinates
 * @property string $view_area
 * @property integer $record
 */
class Cams extends CActiveRecord {

	public static function model($className=__CLASS__)	{
		return parent::model($className);
	}

	public function tableName()	{
		return 'cams';
	}

	public function rules()	{
		return array(
			array('server_id, user_id, name, url', 'required', 'message' => Yii::t('errors', 'cannot be blank')),
			array('server_id, user_id, show, is_public, record', 'numerical', 'integerOnly'=>true),
			array('url, prev_url', 'length', 'max'=>2000),
			array('time_offset', 'length', 'max'=>4),
			array('coordinates', 'match', 'pattern'=>'/^-?[0-9]{1,3}.[0-9]+, -?[0-9]{1,3}.[0-9]+$/u', 'message' => Yii::t('cams', 'Coordinates must be: "xxx.xxx, yyy.yyy"')),
			array('desc', 'safe'),
			array('view_area', 'safe'),
			array('id, user_id, name, desc, url, prev_url, show, is_public, time_offset, record', 'safe', 'on'=>'search'),
			);
	}

	public function relations()	{
		return array(
			'owner' => array(
				self::BELONGS_TO,
				'Users',
				'user_id',
				'select' => 'nick'
				),
			);
	}

	public function attributeLabels() {
		return array(
			'id' => 'ID',
			'user_id' => 'User',
			'server_id' => Yii::t('cams', 'Server'),
			'name' => Yii::t('cams', 'Name'),
			'desc' => Yii::t('cams', 'Description'),
			'url' => 'URL',
			'prev_url' => Yii::t('cams', 'Low quality stream URL'),
			'show' => Yii::t('cams', 'Show on main page'),
			'public' => Yii::t('cams', 'Public'),
			'time_offset' => Yii::t('cams', 'Time zone'),
			'coordinates' => Yii::t('cams', 'Coordinates'),
			'view_area' => Yii::t('cams', 'View area'),
			'record' => Yii::t('cams', 'Enable record')
			);
	}

	public function getSessionId($low = false) {
		$real_id = $low ? $this->id.'_low' : $this->id;
		$user_ip = Yii::app()->user->isGuest ? 'none' : Yii::app()->user->user_ip;
		$session = Sessions::model()->findByAttributes(array('ip' => $user_ip, 'real_id' => $real_id), array('select' => 'session_id'));
		if(!$session) {
			$session = new Sessions;
			$session->ip = $user_ip;
			$session->real_id = $real_id;
			$session->user_id = Yii::app()->user->isGuest ? 0 : Yii::app()->user->getId();
			$session_key = Yii::app()->user->isGuest ? md5(time().'none'.uniqid()) : Yii::app()->user->session_key;
			$session->session_id = md5($real_id.$session_key);
			$session->save();
		} 
		return $session->session_id;
	}

	public function getRealId($session_id) {
		$session = Sessions::model()->findByAttributes(array('session_id' => $session_id), array('select' => 'real_id'));
		if(!$session) {
			return false;
		} 
		return $session->real_id;
	}

	public function afterDelete() {
		parent::afterDelete();
		$shared = Shared::model()->findAllByAttributes(array('cam_id' => $this->id));
		foreach ($shared as $s) {
			Notifications::model()->deleteAllByAttributes(array('shared_id' => $s->id, 'status' => 1));
			$s->delete();
		}
		Sessions::model()->deleteAllByAttributes(array('real_id' => $this->id));
		Sessions::model()->deleteAllByAttributes(array('real_id' => $this->id . '_low'));
		return true;
		
	}

	public static function getAvailableCams() {
		$id = Yii::app()->user->getId();
		$public = array();
		$publicAll = Cams::model()->findAllByAttributes(array('is_public' => 1));
		$shared = Shared::model()->findAllByAttributes(array('user_id' => $id, 'is_public' => 0));
		foreach ($shared as $key => $value) {
			if(!isset($value->cam->id)) {
				$value->delete();
				unset($shared[$key]);
			}
		}
		$publicEdited = Shared::model()->findAllByAttributes(array('user_id' => $id, 'is_public' => 1), array('index' => 'cam_id'));
		foreach ($publicAll as $cam) {
			if(isset($publicEdited[$cam->id])) {
				$public[] = $publicEdited[$cam->id];
			} else {
				$public[] = $cam;
			}
		}
		$myCams = Cams::model()->findAllByAttributes(array('user_id' => $id));
		$myPublicCams = $public;
		$mySharedCams = $shared;
		$allMyCams = array();
		foreach ($myCams as $key => $cam) {
			$allMyCams[$cam->id] = $cam;
		}
		foreach ($myPublicCams as $key => $cam) {
			if(isset($cam->cam)) {
				$allMyCams[$cam->id] = $cam->cam;
			} else {
				$allMyCams[$cam->id] = $cam;
			}
		}
		foreach ($mySharedCams as $key => $cam) {
			$allMyCams[$cam->cam_id] = $cam->cam;
		}
		return $allMyCams;
	}
}
