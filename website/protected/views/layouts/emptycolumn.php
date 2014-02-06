<?php /* @var $this Controller */ ?>
<head>
	<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
	<title><?php echo CHtml::encode($this->pageTitle); ?></title>
</head>

<?php $this->beginContent('//layouts/empty'); ?>
	<?php echo $content; ?>
<?php $this->endContent(); ?>