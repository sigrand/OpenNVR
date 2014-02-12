<?php

class ScreensController extends Controller
{
	/**
	 * @var string the default layout for the views. Defaults to '//layouts/column2', meaning
	 * using two-column layout. See 'protected/views/layouts/column2.php'.
	 */
	public $layout='//layouts/column2';

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
		foreach ($myCams as $key => $cam) {
			$select_options[$cam->id] = $cam->name;
		}
		return $select_options;
	}

	/**
	 * Creates a new model.
	 * If creation is successful, the browser will be redirected to the 'view' page.
	 */
	public function actionCreate()
	{
		$model=new Screens;
		$model->name = "Название экрана";

		// Uncomment the following line if AJAX validation is needed
		// $this->performAjaxValidation($model);


		if(isset($_POST['Screens']))
		{
			$model->attributes=$_POST['Screens'];
			$model->owner_id = Yii::app()->user->getId();
			if($model->save()) {
				Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => 'Экран успешно добавлен'));
				$this->redirect(array('manage'));
			} else {
				Yii::app()->user->setFlash('notify', array('type' => 'error', 'message' => 'Ошибка при добавлении экрана'));
				$this->redirect(array('manage'));
			}
		}

		$this->render('create',array(
			'model'=>$model,
			'myCams' => $this->getAvailableCams()
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
		Yii::app()->user->setFlash('notify', array('type' => 'success', 'message' => 'Экран успешно удален'));
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
		$model=Screens::model()->findByPk($id);
		if($model===null) {
				throw new CHttpException(404,'The requested page does not exist.');
		} elseif ((Yii::app()->user->getId() != $model->owner_id) && (Yii::app()->user->permissions != 3)) {
			$model=null;
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
