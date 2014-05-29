<li><?php echo CHtml::link(Yii::t('menu', 'Servers'), $this->createUrl('admin/servers')); ?></li>
<li><?php echo CHtml::link(Yii::t('menu', 'Public cams'), $this->createUrl('admin/cams')); ?></li>
<li><?php echo CHtml::link(Yii::t('menu', 'Users'), $this->createUrl('admin/users')); ?></li>
<li><?php echo CHtml::link(Yii::t('menu', 'Logs'), $this->createUrl('admin/logs', array('type' => 'system'))); ?></li>
<li><?php echo CHtml::link(Yii::t('menu', 'Settings'), $this->createUrl('admin/settings')); ?></li>
<li><?php echo CHtml::link(Yii::t('menu', 'Updater'), $this->createUrl('admin/updater')); ?></li>
<li><?php echo CHtml::link(Yii::t('menu', 'Backup Manager'), $this->createUrl('admin/backupmanager')); ?></li>