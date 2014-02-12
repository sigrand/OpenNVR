<?php
/* @var $this CamsController */
?>
<div class="col-sm-12">
	<?php
	if(Yii::app()->user->hasFlash('notify')) {
		$notify = Yii::app()->user->getFlash('notify');
		?>
		<div class="alert alert-<?php echo $notify['type']; ?>"><?php echo $notify['message']; ?>.</div>
		<?php } ?>
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title"><?php echo Yii::t('cams', 'Мои камеры'); ?></h3>
			</div>
			<div class="panel-body">
				<?php
				$addLink = CHtml::link('<img src="'.Yii::app()->request->baseUrl.'/images/add-icon.png" style="width:32px;"> '.Yii::t('cams', 'Добавить камеру'), $this->createUrl('cams/add'));
				if(!empty($myCams)) {
					echo $addLink;
					?>
					<table class="table table-striped">
						<thead>
							<th><?php echo CHtml::activeCheckBox($form, "checkAll", array ("class" => "checkAll")); ?></th>
							<th>#</th>
							<th><?php echo Yii::t('cams', 'Название'); ?></th>
							<th><?php echo Yii::t('cams', 'Описание'); ?></th>
							<th><?php echo Yii::t('cams', 'Показывать?'); ?></th>
							<th><?php echo Yii::t('cams', 'Записывать?'); ?></th>
							<th><?php echo Yii::t('cams', 'Редактировать'); ?></th>
							<th><?php echo Yii::t('cams', 'Удалить'); ?></th>
						</thead>
						<tbody>
							<?php
							echo CHtml::beginForm($this->createUrl('cams/manage'), "post");
							foreach ($myCams as $key => $cam) {
								echo '<tr>
								<td>'.CHtml::activeCheckBox($form, 'cam_'.$cam->id).'</td>
								<td>'.($key+1).'</td>
								<td>'.CHtml::link(CHtml::encode($cam->name), $this->createUrl('cams/fullscreen', array('id' => $cam->id)), array('target' => '_blank')).'</td>
								<td>'.CHtml::encode($cam->desc).'</td>
								<td>'.($cam->show ? Yii::t('cams', 'Показывать') : Yii::t('cams', 'Не показывать')).'</td>
								<td>'.($cam->record ? Yii::t('cams', 'Записывать') : Yii::t('cams', 'Не записывать')).'</td>
								<td>'.CHtml::link(Yii::t('cams', 'Редактировать'), $this->createUrl('cams/edit', array('id' => $cam->id))).'</td>
								<td>'.CHtml::link(Yii::t('cams', 'Удалить'), $this->createUrl('cams/delete', array('id' => $cam->id))).'</td>
								</tr>';
							}
							?>
							<tr>
								<td colspan="4"></td>
								<td><?php echo CHtml::submitButton(Yii::t('cams', 'Расшарить'), array('name' => 'share', 'class' => 'btn btn-success')); ?></td>
								<td><?php echo CHtml::submitButton(Yii::t('cams', 'Показывать/Не показывать'), array('name' => 'show', 'class' => 'btn btn-primary')); ?></td>
								<td><?php echo CHtml::submitButton(Yii::t('cams', 'Записывать/Не записывать'), array('name' => 'record', 'class' => 'btn btn-warning')); ?></td>
								<td><?php echo CHtml::submitButton(Yii::t('cams', 'Удалить'), array('name' => 'del', 'class' => 'btn btn-danger')); ?></td>
							</tr>
							<?php echo CHtml::endForm(); ?>
						</tbody>
					</table>
					<?php
				} else {
					echo Yii::t('cams', '<br/>Список моих камер пуст.<br/>');
				}
				echo $addLink;
				?>
			</div>
		</div>
	</div>
	<div class="col-sm-12">
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title"><?php echo Yii::t('cams', 'Расшаренные для меня камеры'); ?></h3>
			</div>
			<div class="panel-body">
				<?php
				if(!empty($mySharedCams)) {
					?>
					<table class="table table-striped">
						<thead>
							<th><?php echo CHtml::activeCheckBox($form, "checkAllSh", array ("class" => "checkAllSh")); ?></th>
							<th>#</th>
							<th><?php echo Yii::t('cams', 'Название'); ?></th>
							<th><?php echo Yii::t('cams', 'Хозяин'); ?></th>
							<th><?php echo Yii::t('cams', 'Описание'); ?></th>
							<th><?php echo Yii::t('cams', 'Показывать?'); ?></th>
							<th><?php echo Yii::t('cams', 'Удалить'); ?></th>
						</thead>
						<tbody>
							<?php
							echo CHtml::beginForm($this->createUrl('cams/manage'), "post");
							foreach ($mySharedCams as $key => $cam) {
								echo '<tr>
								<td>'.CHtml::activeCheckBox($form, 'shcam_'.$cam->id).'</td>
								<td>'.($key + 1).'</td>
								<td>'.CHtml::link(CHtml::encode($cam->cam->name), $this->createUrl('cams/fullscreen', array('id' => $cam->cam->id)), array('target' => '_blank')).'</td>
								<td>'.CHtml::encode($cam->owner->nick).'</td>
								<td>'.CHtml::encode($cam->cam->desc).'</td>
								<td>'.($cam->show ? Yii::t('cams', 'Показывать') : Yii::t('cams', 'Не показывать')).'</td>
								<td>'.CHtml::link(Yii::t('cams', 'Удалить'), $this->createUrl('cams/delete', array('id' => $cam->id, 'type' => 'share'))).'</td>
								</tr>';
							}
							?>
							<tr>
								<td colspan="5"></td>
								<td><?php echo CHtml::submitButton(Yii::t('cams', 'Показывать/Не показывать'), array('name' => 'show', 'class' => 'btn btn-primary')); ?></td>
								<td><?php echo CHtml::submitButton(Yii::t('cams', 'Удалить'), array('name' => 'del', 'class' => 'btn btn-danger')); ?></td>
							</tr>
							<?php echo CHtml::endForm(); ?>
						</tbody>
					</table>
					<?php
				} else {
					echo Yii::t('cams', 'Список расшареных, для меня камер пуст.<br/>');
				}
				?>
			</div>
		</div>
	</div>
	<div class="col-sm-12">
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title"><?php echo Yii::t('cams', 'Публичные камеры'); ?></h3>
			</div>
			<div class="panel-body">
				<?php
				if(!empty($myPublicCams)) {
					?>
					<table class="table table-striped">
						<thead>
							<tr>
								<th><?php echo CHtml::activeCheckBox($form, "checkAllP", array ("class" => "checkAllP")); ?></th>
								<th>#</th>
								<th><?php echo Yii::t('cams', 'Название'); ?></th>
								<th><?php echo Yii::t('cams', 'Хозяин'); ?></th>
								<th><?php echo Yii::t('cams', 'Описание'); ?></th>
								<th><?php echo Yii::t('cams', 'Показывать?'); ?></th>
							</tr>
						</thead>
						<tbody>
							<?php
							echo CHtml::beginForm($this->createUrl('cams/manage'), "post");
							foreach ($myPublicCams as $key => $cam) {
								if(isset($cam->cam_id)) {
									$cam = $cam->cam;
									$cam->show = $myPublicCams[$key]->show;
									//$cam->owner = $myPublicCams[$key]->owner->nick;
								} else {
									$cam->show = 1;
								}
								echo '<tr>
								<td>'.CHtml::activeCheckBox($form, 'pcam_'.$cam->id).'</td>
								<td>'.($key + 1).'</td>
								<td>'.CHtml::link(CHtml::encode($cam->name), $this->createUrl('cams/fullscreen', array('id' => $cam->id)), array('target' => '_blank')).'</td>
								<td>'.CHtml::encode($cam->owner->nick).'</td>
								<td>'.CHtml::encode($cam->desc).'</td>
								<td>'.($cam->show ? Yii::t('cams', 'Показывать') : Yii::t('cams', 'Не показывать')).'</td>
								</tr>';
							}
							?>
							<tr>
								<td colspan="5"></td>
								<td><?php echo CHtml::submitButton(Yii::t('cams', 'Показывать/Не показывать'), array('name' => 'show', 'class' => 'btn btn-primary')); ?></td>
							</tr>
							<?php echo CHtml::endForm(); ?>
						</tbody>
					</table>
					<?php
				} else {
					echo Yii::t('cams', 'Список публичных камер пуст.<br/>');
				}
				?>
			</div>
		</div>
	</div>
	<script>
	$(document).ready(function(){
		$(".checkAll").click(function(){
			$('input[id*="CamsForm_cam_"]').not(this).prop('checked', this.checked);
		});
		$(".checkAllSh").click(function(){
			$('input[id*="CamsForm_shcam_"]').not(this).prop('checked', this.checked);
		});
		$(".checkAllP").click(function(){
			$('input[id*="CamsForm_pcam_"]').not(this).prop('checked', this.checked);
		});
	});
	</script>