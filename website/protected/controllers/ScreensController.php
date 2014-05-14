<?php

class ScreensController extends Controller
{
	/**
	 * @var string the default layout for the views. Defaults to '//layouts/column2', meaning
	 * using two-column layout. See 'protected/views/layouts/column2.php'.
	 */
	//public $layout='//layouts/column2';

	/**
	 * @return array action filters
	 */
	public function filters()
	{
		return array(
			'accessControl', // perform access control for CRUD operations
		);
	}

	/**
	 * Specifies the access control rules.
	 * This method is used by the 'accessControl' filter.
	 * @return array access control rules
	 */
	public function accessRules()
	{
		return array(
			array('allow',
				'actions'=>array('view'),
				'users'=>array('*'),
			),
			array('allow', // allow authenticated user to perform 'create' and 'update' actions
				'actions'=>array('index','view','create','update','manage','delete'),
				'users'=>array('@'),
			),
			array('deny',  // deny all users
				'users'=>array('*'),
			),
		);
	}

	/**
	 * Displays a particular model.
	 * @param integer $id the ID of the model to be displayed
	 */
	public function actionView($id, $fullscreen=false)
	{
		if ($fullscreen == "true") {
			$this->layout = 'emptycolumn';
		}
		$model=$this->loadModel($id);
		if ($model) {
			$this->pageTitle = $model->name;
			$this->render('view',array(
				'model'=>$model,
				'fullscreen'=>$fullscreen,
			));
		}
	}

	private function getAvailableCams() {
		$myCams = Cams::getAvailableCams();
		$select_options = array();
		foreach ($myCams as $key => $cam) {
			$select_options[$cam->id] = $cam->name;
		}
		return $select_options;
	}

	/**
	 * Creates a new model.
	 * If creation is successful, the browser will be redirected to the 'view' page.
	 */
	public function actionCreate($type='')
	{
		$model=new Screens;
		$model->name = Yii::t('screens', 'Screen name');

		// Uncomment the following line if AJAX validation is needed
		// $this->performAjaxValidation($model);


		if(isset($_POST['Screens']))
		{
			$model->attributes=$_POST['Screens'];
			$model->owner_id = Yii::app()->user->getId();
			if($model->save()) {
				Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('screens', 'Screen successfully added')));
				$this->redirect(array('manage'));
			} else {
				Yii::app()->user->setFlash('notify', array('type' => 'error', 'message' => Yii::t('screens', 'Error add screen')));
				$this->redirect(array('manage'));
			}
		} else {
			switch ($type) {
				case 'custom':
				break;
				case '1':
					$model->cam1_x=0;
					$model->cam1_y=0;
					$model->cam1_w=100;
					$model->cam1_h=100;
				break;
				case '2x2':
					$model->cam1_x=0;
					$model->cam1_y=0;
					$model->cam1_w=50;
					$model->cam1_h=50;
					$model->cam2_x=50;
					$model->cam2_y=0;
					$model->cam2_w=50;
					$model->cam2_h=50;
					$model->cam3_x=0;
					$model->cam3_y=50;
					$model->cam3_w=50;
					$model->cam3_h=50;
					$model->cam4_x=50;
					$model->cam4_y=50;
					$model->cam4_w=50;
					$model->cam4_h=50;
				break;
				case '3x3':
					$model->cam1_x=0;
					$model->cam1_y=0;
					$model->cam1_w=33.333333;
					$model->cam1_h=33.333333;
					$model->cam2_x=0;
					$model->cam2_y=33.333333;
					$model->cam2_w=33.333333;
					$model->cam2_h=33.333333;
					$model->cam3_x=0;
					$model->cam3_y=66.666666;
					$model->cam3_w=33.333333;
					$model->cam3_h=33.333333;
					$model->cam4_x=33.333333;
					$model->cam4_y=0;
					$model->cam4_w=33.333333;
					$model->cam4_h=33.333333;
					$model->cam5_x=33.333333;
					$model->cam5_y=33.333333;
					$model->cam5_w=33.333333;
					$model->cam5_h=33.333333;
					$model->cam6_x=33.333333;
					$model->cam6_y=66.666666;
					$model->cam6_w=33.333333;
					$model->cam6_h=33.333333;
					$model->cam7_x=66.666666;
					$model->cam7_y=0;
					$model->cam7_w=33.333333;
					$model->cam7_h=33.333333;
					$model->cam8_x=66.666666;
					$model->cam8_y=33.333333;
					$model->cam8_w=33.333333;
					$model->cam8_h=33.333333;
					$model->cam9_x=66.666666;
					$model->cam9_y=66.666666;
					$model->cam9_w=33.333333;
					$model->cam9_h=33.333333;
				break;
				case '4x4':
					$model->cam1_x=0;
					$model->cam1_y=0;
					$model->cam1_w=25;
					$model->cam1_h=25;
					$model->cam2_x=0;
					$model->cam2_y=25;
					$model->cam2_w=25;
					$model->cam2_h=25;
					$model->cam3_x=0;
					$model->cam3_y=50;
					$model->cam3_w=25;
					$model->cam3_h=25;
					$model->cam4_x=0;
					$model->cam4_y=75;
					$model->cam4_w=25;
					$model->cam4_h=25;

					$model->cam5_x=25;
					$model->cam5_y=0;
					$model->cam5_w=25;
					$model->cam5_h=25;
					$model->cam6_x=25;
					$model->cam6_y=25;
					$model->cam6_w=25;
					$model->cam6_h=25;
					$model->cam7_x=25;
					$model->cam7_y=50;
					$model->cam7_w=25;
					$model->cam7_h=25;
					$model->cam8_x=25;
					$model->cam8_y=75;
					$model->cam8_w=25;
					$model->cam8_h=25;

					$model->cam9_x=50;
					$model->cam9_y=0;
					$model->cam9_w=25;
					$model->cam9_h=25;
					$model->cam10_x=50;
					$model->cam10_y=25;
					$model->cam10_w=25;
					$model->cam10_h=25;
					$model->cam11_x=50;
					$model->cam11_y=50;
					$model->cam11_w=25;
					$model->cam11_h=25;
					$model->cam12_x=50;
					$model->cam12_y=75;
					$model->cam12_w=25;
					$model->cam12_h=25;

					$model->cam13_x=75;
					$model->cam13_y=0;
					$model->cam13_w=25;
					$model->cam13_h=25;
					$model->cam14_x=75;
					$model->cam14_y=25;
					$model->cam14_w=25;
					$model->cam14_h=25;
					$model->cam15_x=75;
					$model->cam15_y=50;
					$model->cam15_w=25;
					$model->cam15_h=25;
					$model->cam16_x=75;
					$model->cam16_y=75;
					$model->cam16_w=25;
					$model->cam16_h=25;

				break;
				default:
					$this->render('select_screen_type');
					return;
				break;
			}
		}

		$this->render('create',array(
			'model'=>$model,
			'myCams' => $this->getAvailableCams(),
			'type' => $type
		));
	}

	public function actionUpdate($id)
	{
		$model=$this->loadModel($id);

		if ($model) {

		if(isset($_POST['Screens']))
		{
			$model->attributes=$_POST['Screens'];
			if($model->save())
				$this->redirect(array('manage'));
		}

		$this->render('update',array(
			'model'=>$model,
			'myCams' => $this->getAvailableCams()
		));

		}
	}

	public function actionDelete($id)
	{
		$model = $this->loadModel($id);
		if ($model) {
		Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => Yii::t('screens', 'Screen successfully deleted')));
		$model->delete();

		// if AJAX request (triggered by deletion via admin grid view), we should not redirect the browser
		if(!isset($_GET['ajax']))
			$this->redirect(isset($_POST['returnUrl']) ? $_POST['returnUrl'] : array('manage'));

		}
	}

	/**
	 * Lists all models.
	 */
	public function actionIndex()
	{
		$dataProvider=new CActiveDataProvider('Screens');
		$this->redirect('manage');
	}

	public function actionManage()
	{
		if (Yii::app()->user->permissions == 3) {
			$myscreens = Screens::model()->findAll();
		} else {
			$myscreens = Screens::model()->findAllByAttributes(array('owner_id' => Yii::app()->user->getId()));
		}
		$this->render('manage',array(
			'myscreens'=>$myscreens,
		));
	}

	/**
	 * Returns the data model based on the primary key given in the GET variable.
	 * If the data model is not found, an HTTP exception will be raised.
	 * @param integer $id the ID of the model to be loaded
	 * @return Screens the loaded model
	 * @throws CHttpException
	 */
	public function loadModel($id)
	{
		//$id = (int)$id;
		$model=Screens::model()->findByPk($id);
		if($model===null) {
				throw new CHttpException(404,'The requested page does not exist.');
//		} elseif ((Yii::app()->user->getId() != $model->owner_id) && (Yii::app()->user->permissions != 3)) {
//			$model=null;
		}
		return $model;
	}

	/**
	 * Performs the AJAX validation.
	 * @param Screens $model the model to be validated
	 */
	protected function performAjaxValidation($model)
	{
		if(isset($_POST['ajax']) && $_POST['ajax']==='screens-form')
		{
			echo CActiveForm::validate($model);
			Yii::app()->end();
		}
	}
}
