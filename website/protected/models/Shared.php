<?php

/**
 * This is the model class for table "shared".
 *
 * The followings are the available columns in table 'shared':
 * @property integer $id
 * @property integer $owner_id
 * @property integer $user_id
 * @property integer $cam_id
 * @property integer $show
 * @property integer $is_public
 * @property integer $is_approved
 */
class Shared extends CActiveRecord
{
	/**
	 * Returns the static model of the specified AR class.
	 * @param string $className active record class name.
	 * @return Shared the static model class
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
		return 'shared';
	}

	/**
	 * @return array validation rules for model attributes.
	 */
	public function rules()
	{
		// NOTE: you should only define rules for those attributes that
		// will receive user inputs.
		return array(
			array('owner_id, user_id, cam_id', 'required'),
			array('owner_id, user_id, cam_id, show, is_public, is_approved', 'numerical', 'integerOnly'=>true),
			// The following rule is used by search().
			// Please remove those attributes that should not be searched.
			array('id, owner_id, user_id, cam_id, show, is_public, is_approved', 'safe', 'on'=>'search'),
			);
	}

	/**
	 * @return array relational rules.
	 */
	public function relations()
	{
		// NOTE: you may need to adjust the relation name and the related
		// class name for the relations automatically generated below.
		return array(
			'cam' => array(
				self::BELONGS_TO,
				'Cams',
				'cam_id',
				),
			'owner' => array(
				self::BELONGS_TO,
				'Users',
				'owner_id',
				'select' => 'nick,status'
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
			'owner_id' => 'Owner',
			'user_id' => 'User',
			'cam_id' => 'Cam',
			'show' => 'Show',
			'is_public' => 'Is Public',
			'is_approved' => 'Is Approved',
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
		$criteria->compare('owner_id',$this->owner_id);
		$criteria->compare('user_id',$this->user_id);
		$criteria->compare('cam_id',$this->cam_id);
		$criteria->compare('show',$this->show);
		$criteria->compare('is_public',$this->is_public);
		$criteria->compare('is_approved',$this->is_approved);

		return new CActiveDataProvider($this, array(
			'criteria'=>$criteria,
			));
	}
}