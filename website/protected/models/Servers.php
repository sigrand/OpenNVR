<?php

/**
 * This is the model class for table "servers".
 *
 * The followings are the available columns in table 'servers':
 * @property string $id
 * @property string $ip
 * @property string $protocol
 * @property string $s_port
 * @property string $w_port
 * @property string $l_port
 */
class Servers extends CActiveRecord
{
	/**
	 * Returns the static model of the specified AR class.
	 * @param string $className active record class name.
	 * @return Servers the static model class
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
		return 'servers';
	}

	/**
	 * @return array validation rules for model attributes.
	 */
	public function rules()
	{
		// NOTE: you should only define rules for those attributes that
		// will receive user inputs.
		return array(
			array('ip, protocol, s_port, w_port, l_port', 'required'),
			array('s_port, w_port, l_port', 'length', 'max'=>5),
			// The following rule is used by search().
			// Please remove those attributes that should not be searched.
			array('id, ip, protocol, s_port, w_port, l_port', 'safe', 'on'=>'search'),
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
			'ip' => Yii::t('admin', 'ip'),
			'protocol' => Yii::t('admin', 'protocol'),
			's_port' => Yii::t('admin', 'server port'),
			'w_port' => Yii::t('admin', 'web port'),
			'l_port' => Yii::t('admin', 'live port'),
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

		$criteria->compare('id',$this->id,true);
		$criteria->compare('ip',$this->ip,true);
		$criteria->compare('protocol',$this->protocol,true);
		$criteria->compare('s_port',$this->s_port,true);
		$criteria->compare('w_port',$this->w_port,true);
		$criteria->compare('l_port',$this->l_port,true);

		return new CActiveDataProvider($this, array(
			'criteria'=>$criteria,
		));
	}
}