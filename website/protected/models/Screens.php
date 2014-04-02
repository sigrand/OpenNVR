<?php

class Screens extends CActiveRecord
{
	/**
	 * Returns the static model of the specified AR class.
	 * @param string $className active record class name.
	 * @return Screens the static model class
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
		return 'screens';
	}

	/**
	 * @return array validation rules for model attributes.
	 */
	public function rules()
	{
		// NOTE: you should only define rules for those attributes that
		// will receive user inputs.
		return array(
			array('name, owner_id, cam1_id, cam1_x, cam1_y, cam1_w, cam1_h', 'required'),
			array('owner_id, cam1_id, cam1_x, cam1_y, cam1_w, cam1_h, cam2_id, cam2_x, cam2_y, cam2_w, cam2_h, cam3_id, cam3_x, cam3_y, cam3_w, cam3_h, cam4_id, cam4_x, cam4_y, cam4_w, cam4_h, cam5_id, cam5_x, cam5_y, cam5_w, cam5_h, cam6_id, cam6_x, cam6_y, cam6_w, cam6_h, cam7_id, cam7_x, cam7_y, cam7_w, cam7_h, cam8_id, cam8_x, cam8_y, cam8_w, cam8_h, cam9_id, cam9_x, cam9_y, cam9_w, cam9_h, cam10_id, cam10_x, cam10_y, cam10_w, cam10_h, cam11_id, cam11_x, cam11_y, cam11_w, cam11_h, cam12_id, cam12_x, cam12_y, cam12_w, cam12_h, cam13_id, cam13_x, cam13_y, cam13_w, cam13_h, cam14_id, cam14_x, cam14_y, cam14_w, cam14_h, cam15_id, cam15_x, cam15_y, cam15_w, cam15_h, cam16_id, cam16_x, cam16_y, cam16_w, cam16_h', 'numerical', 'integerOnly'=>true),
			array('name, cam1_descr, cam2_descr, cam3_descr, cam4_descr, cam5_descr, cam6_descr, cam7_descr, cam8_descr, cam9_descr, cam10_descr, cam11_descr, cam12_descr, cam13_descr, cam14_descr, cam15_descr, cam16_descr', 'length', 'max'=>100),
			// The following rule is used by search().
			// Please remove those attributes that should not be searched.
			array('id, name, owner_id, cam1_id, cam2_id, cam3_id, cam4_id, cam5_id, cam6_id, cam7_id, cam8_id, cam9_id, cam10_id, cam11_id, cam11_x, cam11_y, cam11_w, cam11_h, cam11_descr, cam12_id, cam13_id, cam14_id, cam15_id, cam16_id', 'safe', 'on'=>'search'),
			);
}

	/**
	 * @return array relational rules.
	 */
	public function relations()	{
		return array(
			'owner' => array(
				self::BELONGS_TO,
				'Users',
				'owner_id',
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
			'name' => 'Name',
			'owner_id' => 'Owner',
			'cam1_id' => 'Cam1',
			'cam1_x' => 'Cam1 X',
			'cam1_y' => 'Cam1 Y',
			'cam1_w' => 'Cam1 W',
			'cam1_h' => 'Cam1 H',
			'cam1_descr' => 'Cam1 Descr',
			'cam2_id' => 'Cam2',
			'cam2_x' => 'Cam2 X',
			'cam2_y' => 'Cam2 Y',
			'cam2_w' => 'Cam2 W',
			'cam2_h' => 'Cam2 H',
			'cam2_descr' => 'Cam2 Descr',
			'cam3_id' => 'Cam3',
			'cam3_x' => 'Cam3 X',
			'cam3_y' => 'Cam3 Y',
			'cam3_w' => 'Cam3 W',
			'cam3_h' => 'Cam3 H',
			'cam3_descr' => 'Cam3 Descr',
			'cam4_id' => 'Cam4',
			'cam4_x' => 'Cam4 X',
			'cam4_y' => 'Cam4 Y',
			'cam4_w' => 'Cam4 W',
			'cam4_h' => 'Cam4 H',
			'cam4_descr' => 'Cam4 Descr',
			'cam5_id' => 'Cam5',
			'cam5_x' => 'Cam5 X',
			'cam5_y' => 'Cam5 Y',
			'cam5_w' => 'Cam5 W',
			'cam5_h' => 'Cam5 H',
			'cam5_descr' => 'Cam5 Descr',
			'cam6_id' => 'Cam6',
			'cam6_x' => 'Cam6 X',
			'cam6_y' => 'Cam6 Y',
			'cam6_w' => 'Cam6 W',
			'cam6_h' => 'Cam6 H',
			'cam6_descr' => 'Cam6 Descr',
			'cam7_id' => 'Cam7',
			'cam7_x' => 'Cam7 X',
			'cam7_y' => 'Cam7 Y',
			'cam7_w' => 'Cam7 W',
			'cam7_h' => 'Cam7 H',
			'cam7_descr' => 'Cam7 Descr',
			'cam8_id' => 'Cam8',
			'cam8_x' => 'Cam8 X',
			'cam8_y' => 'Cam8 Y',
			'cam8_w' => 'Cam8 W',
			'cam8_h' => 'Cam8 H',
			'cam8_descr' => 'Cam8 Descr',
			'cam9_id' => 'Cam9',
			'cam9_x' => 'Cam9 X',
			'cam9_y' => 'Cam9 Y',
			'cam9_w' => 'Cam9 W',
			'cam9_h' => 'Cam9 H',
			'cam9_descr' => 'Cam9 Descr',
			'cam10_id' => 'Cam10',
			'cam10_x' => 'Cam10 X',
			'cam10_y' => 'Cam10 Y',
			'cam10_w' => 'Cam10 W',
			'cam10_h' => 'Cam10 H',
			'cam10_descr' => 'Cam10 Descr',
			'cam11_id' => 'Cam11',
			'cam11_x' => 'Cam11 X',
			'cam11_y' => 'Cam11 Y',
			'cam11_w' => 'Cam11 W',
			'cam11_h' => 'Cam11 H',
			'cam11_descr' => 'Cam11 Descr',
			'cam12_id' => 'Cam12',
			'cam12_x' => 'Cam12 X',
			'cam12_y' => 'Cam12 Y',
			'cam12_w' => 'Cam12 W',
			'cam12_h' => 'Cam12 H',
			'cam12_descr' => 'Cam12 Descr',
			'cam13_id' => 'Cam13',
			'cam13_x' => 'Cam13 X',
			'cam13_y' => 'Cam13 Y',
			'cam13_w' => 'Cam13 W',
			'cam13_h' => 'Cam13 H',
			'cam13_descr' => 'Cam13 Descr',
			'cam14_id' => 'Cam14',
			'cam14_x' => 'Cam14 X',
			'cam14_y' => 'Cam14 Y',
			'cam14_w' => 'Cam14 W',
			'cam14_h' => 'Cam14 H',
			'cam14_descr' => 'Cam14 Descr',
			'cam15_id' => 'Cam15',
			'cam15_x' => 'Cam15 X',
			'cam15_y' => 'Cam15 Y',
			'cam15_w' => 'Cam15 W',
			'cam15_h' => 'Cam15 H',
			'cam15_descr' => 'Cam15 Descr',
			'cam16_id' => 'Cam16',
			'cam16_x' => 'Cam16 X',
			'cam16_y' => 'Cam16 Y',
			'cam16_w' => 'Cam16 W',
			'cam16_h' => 'Cam16 H',
			'cam16_descr' => 'Cam16 Descr',
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
		$criteria->compare('name',$this->name,true);
		$criteria->compare('owner_id',$this->owner_id);
		$criteria->compare('cam1_id',$this->cam1_id);
		$criteria->compare('cam1_x',$this->cam1_x);
		$criteria->compare('cam1_y',$this->cam1_y);
		$criteria->compare('cam1_w',$this->cam1_w);
		$criteria->compare('cam1_h',$this->cam1_h);
		$criteria->compare('cam1_descr',$this->cam1_descr,true);
		$criteria->compare('cam2_id',$this->cam2_id);
		$criteria->compare('cam2_x',$this->cam2_x);
		$criteria->compare('cam2_y',$this->cam2_y);
		$criteria->compare('cam2_w',$this->cam2_w);
		$criteria->compare('cam2_h',$this->cam2_h);
		$criteria->compare('cam2_descr',$this->cam2_descr,true);
		$criteria->compare('cam3_id',$this->cam3_id);
		$criteria->compare('cam3_x',$this->cam3_x);
		$criteria->compare('cam3_y',$this->cam3_y);
		$criteria->compare('cam3_w',$this->cam3_w);
		$criteria->compare('cam3_h',$this->cam3_h);
		$criteria->compare('cam3_descr',$this->cam3_descr,true);
		$criteria->compare('cam4_id',$this->cam4_id);
		$criteria->compare('cam4_x',$this->cam4_x);
		$criteria->compare('cam4_y',$this->cam4_y);
		$criteria->compare('cam4_w',$this->cam4_w);
		$criteria->compare('cam4_h',$this->cam4_h);
		$criteria->compare('cam4_descr',$this->cam4_descr,true);
		$criteria->compare('cam5_id',$this->cam5_id);
		$criteria->compare('cam5_x',$this->cam5_x);
		$criteria->compare('cam5_y',$this->cam5_y);
		$criteria->compare('cam5_w',$this->cam5_w);
		$criteria->compare('cam5_h',$this->cam5_h);
		$criteria->compare('cam5_descr',$this->cam5_descr,true);
		$criteria->compare('cam6_id',$this->cam6_id);
		$criteria->compare('cam6_x',$this->cam6_x);
		$criteria->compare('cam6_y',$this->cam6_y);
		$criteria->compare('cam6_w',$this->cam6_w);
		$criteria->compare('cam6_h',$this->cam6_h);
		$criteria->compare('cam6_descr',$this->cam6_descr,true);
		$criteria->compare('cam7_id',$this->cam7_id);
		$criteria->compare('cam7_x',$this->cam7_x);
		$criteria->compare('cam7_y',$this->cam7_y);
		$criteria->compare('cam7_w',$this->cam7_w);
		$criteria->compare('cam7_h',$this->cam7_h);
		$criteria->compare('cam7_descr',$this->cam7_descr,true);
		$criteria->compare('cam8_id',$this->cam8_id);
		$criteria->compare('cam8_x',$this->cam8_x);
		$criteria->compare('cam8_y',$this->cam8_y);
		$criteria->compare('cam8_w',$this->cam8_w);
		$criteria->compare('cam8_h',$this->cam8_h);
		$criteria->compare('cam8_descr',$this->cam8_descr,true);
		$criteria->compare('cam9_id',$this->cam9_id);
		$criteria->compare('cam9_x',$this->cam9_x);
		$criteria->compare('cam9_y',$this->cam9_y);
		$criteria->compare('cam9_w',$this->cam9_w);
		$criteria->compare('cam9_h',$this->cam9_h);
		$criteria->compare('cam9_descr',$this->cam9_descr,true);
		$criteria->compare('cam10_id',$this->cam10_id);
		$criteria->compare('cam10_x',$this->cam10_x);
		$criteria->compare('cam10_y',$this->cam10_y);
		$criteria->compare('cam10_w',$this->cam10_w);
		$criteria->compare('cam10_h',$this->cam10_h);
		$criteria->compare('cam10_descr',$this->cam10_descr,true);
		$criteria->compare('cam11_id',$this->cam11_id);
		$criteria->compare('cam11_x',$this->cam11_x);
		$criteria->compare('cam11_y',$this->cam11_y);
		$criteria->compare('cam11_w',$this->cam11_w);
		$criteria->compare('cam11_h',$this->cam11_h);
		$criteria->compare('cam11_descr',$this->cam11_descr,true);
		$criteria->compare('cam12_id',$this->cam12_id);
		$criteria->compare('cam12_x',$this->cam12_x);
		$criteria->compare('cam12_y',$this->cam12_y);
		$criteria->compare('cam12_w',$this->cam12_w);
		$criteria->compare('cam12_h',$this->cam12_h);
		$criteria->compare('cam12_descr',$this->cam12_descr,true);
		$criteria->compare('cam13_id',$this->cam13_id);
		$criteria->compare('cam13_x',$this->cam13_x);
		$criteria->compare('cam13_y',$this->cam13_y);
		$criteria->compare('cam13_w',$this->cam13_w);
		$criteria->compare('cam13_h',$this->cam13_h);
		$criteria->compare('cam13_descr',$this->cam13_descr,true);
		$criteria->compare('cam14_id',$this->cam14_id);
		$criteria->compare('cam14_x',$this->cam14_x);
		$criteria->compare('cam14_y',$this->cam14_y);
		$criteria->compare('cam14_w',$this->cam14_w);
		$criteria->compare('cam14_h',$this->cam14_h);
		$criteria->compare('cam14_descr',$this->cam14_descr,true);
		$criteria->compare('cam15_id',$this->cam15_id);
		$criteria->compare('cam15_x',$this->cam15_x);
		$criteria->compare('cam15_y',$this->cam15_y);
		$criteria->compare('cam15_w',$this->cam15_w);
		$criteria->compare('cam15_h',$this->cam15_h);
		$criteria->compare('cam15_descr',$this->cam15_descr,true);
		$criteria->compare('cam16_id',$this->cam16_id);
		$criteria->compare('cam16_x',$this->cam16_x);
		$criteria->compare('cam16_y',$this->cam16_y);
		$criteria->compare('cam16_w',$this->cam16_w);
		$criteria->compare('cam16_h',$this->cam16_h);
		$criteria->compare('cam16_descr',$this->cam16_descr,true);

		return new CActiveDataProvider($this, array(
			'criteria'=>$criteria,
			));
	}
}