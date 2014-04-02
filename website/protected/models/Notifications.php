<?php

/**
 * This is the model class for table "notifications".
 *
 * The followings are the available columns in table 'notifications':
 * @property integer $id
 * @property string $msg
 * @property integer $creator_id
 * @property integer $dest_id
 * @property integer $shared_id
 * @property integer $status
 * @property integer $time
 */
class Notifications extends CActiveRecord
{
	/**
	 * Returns the static model of the specified AR class.
	 * @param string $className active record class name.
	 * @return Notifications the static model class
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
		return 'notifications';
	}

	/**
	 * @return array validation rules for model attributes.
	 */
	public function rules()
	{
		// NOTE: you should only define rules for those attributes that
		// will receive user inputs.
		return array(
			array('msg, time', 'required'),
			array('creator_id, dest_id, shared_id, status, time', 'numerical', 'integerOnly'=>true),
			// The following rule is used by search().
			// Please remove those attributes that should not be searched.
			array('id, msg, creator_id, dest_id, shared_id, status, time', 'safe'),
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
		);
	}

	/**
	 * @return array customized attribute labels (name=>label)
	 */
	public function attributeLabels()
	{
		return array(
			'id' => 'ID',
			'msg' => 'Msg',
			'creator_id' => 'Creator',
			'dest_id' => 'Dest',
			'shared_id' => 'Shared',
			'status' => 'Status',
			'time' => 'Time',
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
		$criteria->compare('msg',$this->msg,true);
		$criteria->compare('creator_id',$this->creator_id);
		$criteria->compare('dest_id',$this->dest_id);
		$criteria->compare('shared_id',$this->shared_id);
		$criteria->compare('status',$this->status);
		$criteria->compare('time',$this->time);

		return new CActiveDataProvider($this, array(
			'criteria'=>$criteria,
		));
	}
}