<?php
/* @var $this ScreensController */
/* @var $model Screens */

/*
$this->breadcrumbs=array(
	'Screens'=>array('index'),
	'Create',
);

$this->menu=array(
	array('label'=>'List Screens', 'url'=>array('index')),
	array('label'=>'Manage Screens', 'url'=>array('admin')),
);
*/
?>

<!-- <h1>Create Screens</h1> -->

<?php echo $this->renderPartial('_form', array('model'=>$model, 'myCams' => $myCams)); ?>