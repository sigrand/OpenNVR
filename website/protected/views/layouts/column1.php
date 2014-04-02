<?php /* @var $this Controller */ ?>
<?php $this->beginContent('//layouts/main-'.Settings::model()->getValue('style')); ?>
<div id="content">
	<?php echo $content; ?>
</div><!-- content -->
<?php $this->endContent(); ?>