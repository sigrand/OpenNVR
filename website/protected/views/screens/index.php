<?php
/* @var $this ScreensController */
/* @var $dataProvider CActiveDataProvider */

$this->breadcrumbs=array(
	'Screens',
);

$this->menu=array(
	array('label'=>'Create Screens', 'url'=>array('create')),
	array('label'=>'Manage Screens', 'url'=>array('admin')),
);
?>

<h1>Screens</h1>

<?php $this->widget('zii.widgets.CListView', array(
	'dataProvider'=>$dataProvider,
	'itemView'=>'_view',
)); ?>
