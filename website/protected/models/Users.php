<?php

/**
 * This is the model class for table "users".
 *
 * The followings are the available columns in table 'users':
 * @property integer $id
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
	 * Returns the static model of the specified AR class.
	 * @param string $className active record class name.
	 * @return Users the static model class
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
			array('status, options', 'numerical', 'integerOnly'=>true),
			array('nick', 'length', 'max'=>150),
			array('email', 'length', 'max'=>250),
			array('pass', 'length', 'max'=>100),
			array('salt', 'length', 'max'=>32),
			array('time_offset', 'length', 'max'=>4),
			// The following rule is used by search().
			// Please remove those attributes that should not be searched.
			array('id, nick, email, pass, salt, time_offset, status, options', 'safe', 'on'=>'search'),
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
	 * @return CActiveDataProvider the data provider that can return the models based on the search/filter conditions.
	 */
	public function search()
	{
		// Warning: Please modify the following code to remove attributes that
		// should not be searched.

		$criteria=new CDbCriteria;

		$criteria->compare('id',$this->id);
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
}