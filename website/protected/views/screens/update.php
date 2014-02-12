<?php
/* @var $this ScreensController */
/* @var $model Screens */

/*
$this->breadcrumbs=array(
	'Screens'=>array('index'),
	$model->name=>array('view','id'=>$model->id),
	'Update',
);

$this->menu=array(
	array('label'=>'List Screens', 'url'=>array('index')),
	array('label'=>'Create Screens', 'url'=>array('create')),
	array('label'=>'View Screens', 'url'=>array('view', 'id'=>$model->id)),
	array('label'=>'Manage Screens', 'url'=>array('admin')),
);
*/
?>
<!--
<h1>Update Screens <?php echo $model->id; ?></h1>
-->
<?php echo $this->renderPartial('_form', array('model'=>$model, 'myCams' => $myCams)); ?>