<?php

/**
 * This is the model class for table "cams".
 *
 * The followings are the available columns in table 'cams':
 * @property integer $id
 * @property integer $user_id
 * @property string $name
 * @property string $desc
 * @property string $url
 * @property string $prev_url
 * @property integer $show
 * @property integer $public
 * @property string $time_offset
 * @property string $coordinates
 * @property integer $record
 */
class Cams extends CActiveRecord
{
	/**
	 * Returns the static model of the specified AR class.
	 * @param string $className active record class name.
	 * @return Cams the static model class
	 */
	public static function model($className=__CLASS__)
	{
		return parent::model($className);
	}

	/**
	 * @return string the associated database table name
	 */
	public function tableName()
	{
		return 'cams';
	}

	/**
	 * @return array validation rules for model attributes.
	 */
	public function rules()
	{
		// NOTE: you should only define rules for those attributes that
		// will receive user inputs.
		return array(
			array('user_id, name, url', 'required'),
			array('user_id, show, public, record', 'numerical', 'integerOnly'=>true),
			array('url, prev_url', 'length', 'max'=>2000),
			array('time_offset', 'length', 'max'=>4),
			array('coordinates', 'match', 'pattern'=>'/^-?[0-9]{1,3}.[0-9]+, -?[0-9]{1,3}.[0-9]+$/u', 'message'=>Yii::t('cams', 'Coordinates must be "xxx.xxx, yyy.yyy"')),
			array('view_area', 'safe'),
			array('desc', 'safe'),
			// The following rule is used by search().
			// Please remove those attributes that should not be searched.
			array('id, user_id, name, desc, url, prev_url, show, public, time_offset, record', 'safe', 'on'=>'search'),
		);
	}

	public function relations()
	{
		// NOTE: you may need to adjust the relation name and the related
		// class name for the relations automatically generated below.
		return array(
			'owner' => array(
				self::BELONGS_TO,
				'Users',
				'user_id',
				'select' => 'nick'
				),
		);
	}

	/**
	 * @return array customized attribute labels (name=>label)
	 */
	public function attributeLabels()
	{
		return array(
			'id' => 'ID',
			'user_id' => 'User',
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

	/**
	 * Retrieves a list of models based on the current search/filter conditions.
	 * @return CActiveDataProvider the data provider that can return the models based on the search/filter conditions.
	 */
	public function search()
	{
		// Warning: Please modify the following code to remove attributes that
		// should not be searched.

		$criteria=new CDbCriteria;

		$criteria->compare('id',$this->id);
		$criteria->compare('user_id',$this->user_id);
		$criteria->compare('name',$this->name,true);
		$criteria->compare('desc',$this->desc,true);
		$criteria->compare('url',$this->url,true);
		$criteria->compare('prev_url',$this->prev_url,true);
		$criteria->compare('show',$this->show);
		$criteria->compare('public',$this->public);
		$criteria->compare('time_offset',$this->time_offset,true);
		$criteria->compare('record',$this->record);

		return new CActiveDataProvider($this, array(
			'criteria'=>$criteria,
		));
	}

	public function getAvailableCams() {
		$id = Yii::app()->user->getId();
		$public = array();
		$publicAll = Cams::model()->findAllByAttributes(array('public' => 1));
		$shared = Shared::model()->findAllByAttributes(array('user_id' => $id, 'public' => 0));
		foreach ($shared as $key => $value) {
			if(!isset($value->cam->id)) {
				$value->delete();
				unset($shared[$key]);
			}
		}
		$publicEdited = Shared::model()->findAllByAttributes(array('user_id' => $id, 'public' => 1), array('index' => 'cam_id'));
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
			$allMyCams[$cam->id] = $cam;
		}
		foreach ($mySharedCams as $key => $cam) {
			$allMyCams[$cam->cam_id] = $cam->cam;
		}
		return $allMyCams;
	}
}
