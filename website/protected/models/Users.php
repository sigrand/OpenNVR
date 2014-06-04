<?php

/**
 * This is the model class for table "users".
 *
 * The followings are the available columns in table 'users':
 * @property integer $id
 * @property integer $server_id
 * @property integer $quota
 * @property string $nick
 * @property string $email
 * @property string $pass
 * @property string $salt
 * @property string $time_offset
 * @property integer $status
 * @property integer $options
 */
class Users extends CActiveRecord
{
	/**
	 * @return string the associated database table name
	 */
	public function tableName()
	{
		return 'users';
	}

    public function validatePassword($password, $salt) {
        return crypt($password.$salt, $this->pass) === $this->pass;
    }

	/**
	 * @return array validation rules for model attributes.
	 */
	public function rules()
	{
		// NOTE: you should only define rules for those attributes that
		// will receive user inputs.
		return array(
			array('nick, pass, salt, status', 'required'),
			array('server_id, quota, status, options', 'numerical', 'integerOnly'=>true),
			array('nick', 'length', 'max'=>150),
			array('email', 'length', 'max'=>250),
			array('pass', 'length', 'max'=>512),
			array('salt', 'length', 'max'=>32),
			array('time_offset', 'length', 'max'=>4),
			// The following rule is used by search().
			// @todo Please remove those attributes that should not be searched.
			array('id, server_id, quota, nick, email, pass, salt, time_offset, status, options', 'safe', 'on'=>'search'),
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
			'server_id' => 'Server',
			'quota' => 'Quota',
			'nick' => 'Nick',
			'email' => 'Email',
			'pass' => 'Pass',
			'salt' => 'Salt',
			'time_offset' => 'Time Offset',
			'status' => 'Status',
			'options' => 'Options',
		);
	}

	/**
	 * Retrieves a list of models based on the current search/filter conditions.
	 *
	 * Typical usecase:
	 * - Initialize the model fields with values from filter form.
	 * - Execute this method to get CActiveDataProvider instance which will filter
	 * models according to data in model fields.
	 * - Pass data provider to CGridView, CListView or any similar widget.
	 *
	 * @return CActiveDataProvider the data provider that can return the models
	 * based on the search/filter conditions.
	 */
	public function search()
	{
		// @todo Please modify the following code to remove attributes that should not be searched.

		$criteria=new CDbCriteria;

		$criteria->compare('id',$this->id);
		$criteria->compare('server_id',$this->server_id);
		$criteria->compare('nick',$this->nick,true);
		$criteria->compare('email',$this->email,true);
		$criteria->compare('pass',$this->pass,true);
		$criteria->compare('salt',$this->salt,true);
		$criteria->compare('time_offset',$this->time_offset,true);
		$criteria->compare('status',$this->status);
		$criteria->compare('options',$this->options);

		return new CActiveDataProvider($this, array(
			'criteria'=>$criteria,
		));
	}

	/**
	 * Returns the static model of the specified AR class.
	 * Please note that you should have this exact method in all your CActiveRecord descendants!
	 * @param string $className active record class name.
	 * @return Users the static model class
	 */
	public static function model($className=__CLASS__)
	{
		return parent::model($className);
	}
}
