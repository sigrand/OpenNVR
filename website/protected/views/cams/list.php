<div class="col-sm-12">
	<?php
	if(Yii::app()->user->hasFlash('notify')) {
		$notify = Yii::app()->user->getFlash('notify');
		?>
		<div class="alert alert-<?php echo $notify['type']; ?>"><?php echo $notify['message']; ?>.</div>
		<?php } ?>
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title"><?php echo Yii::t('cams', 'My cams'); ?></h3>
			</div>
			<div class="panel-body">
				<?php
				$addLink = CHtml::link(Yii::t('cams', 'Add cam'), $this->createUrl('cams/add'), array('class' => 'btn btn-success'));
				if(!empty($myCams)) {
					echo $addLink;
					?>
					<table class="table table-striped">
						<thead>
							<th><?php echo CHtml::activeCheckBox($form, "checkAll", array ("class" => "checkAll")); ?></th>
							<th>#</th>
							<th><?php echo Yii::t('cams', 'Name'); ?></th>
							<th><?php echo Yii::t('cams', 'IP address'); ?></th>
							<th><?php echo Yii::t('cams', 'Description'); ?></th>
							<th><?php echo Yii::t('cams', 'Show?'); ?></th>
							<th><?php echo Yii::t('cams', 'Record?'); ?></th>
							<th><?php echo Yii::t('cams', 'Edit'); ?></th>
							<th><?php echo Yii::t('cams', 'Delete'); ?></th>
						<tbody>
							<?php
							echo CHtml::beginForm($this->createUrl('cams/manage'), "post");
							foreach ($myCams as $key => $cam) {
								preg_match('@^(?:rtsp://)(?:.+\@)?([^:/]+)@i', $cam->url, $m);
								echo '<tr>
								<td>'.CHtml::activeCheckBox($form, 'cam_'.$cam->getSessionId()).'</td>
								<td>'.($key+1).'</td>
								<td>'.CHtml::link(CHtml::encode($cam->name), $this->createUrl('cams/archive', array('id' => $cam->getSessionId(), 'full' => '1')), array('target' => '_blank')).'</td>
								<td>'.$m[1].'</td>
								<td>'.CHtml::encode($cam->desc).'</td>
								<td>'.($cam->show ? Yii::t('cams', 'Show') : Yii::t('cams', 'Don\'t show')).'</td>
								<td>'.($cam->record ? Yii::t('cams', 'Record') : Yii::t('cams', 'Don\'t record')).'</td>
								<td>'.CHtml::link(Yii::t('cams', 'Edit'), $this->createUrl('cams/edit', array('id' => $cam->getSessionId())), array('name' => 'show', 'class' => 'btn btn-primary')).'</td>
								<td>'.CHtml::link(Yii::t('cams', 'Delete'), $this->createUrl('cams/delete', array('id' => $cam->getSessionId())), array('name' => 'del', 'class' => 'btn btn-danger')).'</td>
								</tr>';
							}
							?>
							<tr>
								<td colspan="5"><?php echo Yii::t('cams', 'Mass actions: '); ?></td>
								<td><?php echo CHtml::submitButton(Yii::t('cams', 'Share'), array('name' => 'share', 'class' => 'btn btn-success')); ?></td>
								<td><?php echo CHtml::submitButton(Yii::t('cams', 'Show/Don\'t show'), array('name' => 'show', 'class' => 'btn btn-primary')); ?></td>
								<td><?php echo CHtml::submitButton(Yii::t('cams', 'Record/Don\'t record'), array('name' => 'record', 'class' => 'btn btn-warning')); ?></td>
								<td><?php echo CHtml::submitButton(Yii::t('cams', 'Delete'), array('name' => 'del', 'class' => 'btn btn-danger')); ?></td>
							</tr>
							<?php echo CHtml::endForm(); ?>
						</tbody>
					</table>
					<?php
				} else {
					echo Yii::t('cams', '<br/>My cams list is empty.<br/>');
				}
				echo $addLink;
				?>
			</div>
		</div>
	</div>
	<div class="col-sm-12">
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title"><?php echo Yii::t('cams', 'Cams shared for me'); ?></h3>
			</div>
			<div class="panel-body">
				<?php
				if(!empty($mySharedCams)) {
					?>
					<table class="table table-striped">
						<thead>
							<th><?php echo CHtml::activeCheckBox($form, "checkAllSh", array ("class" => "checkAllSh")); ?></th>
							<th>#</th>
							<th><?php echo Yii::t('cams', 'Name'); ?></th>
							<th><?php echo Yii::t('cams', 'Owner'); ?></th>
							<th><?php echo Yii::t('cams', 'Description'); ?></th>
							<th><?php echo Yii::t('cams', 'Show?'); ?></th>
							<th><?php echo Yii::t('cams', 'Delete'); ?></th>
						</thead>
						<tbody>
							<?php
							echo CHtml::beginForm($this->createUrl('cams/manage'), "post");
							foreach ($mySharedCams as $key => $cam) {
								echo '<tr>
								<td>'.CHtml::activeCheckBox($form, 'shcam_'.$cam->id).'</td>
								<td>'.($key + 1).'</td>
								<td>'.CHtml::link(CHtml::encode($cam->cam->name), $this->createUrl('cams/archive', array('id' => $cam->cam->getSessionId(), 'full' => '1')), array('target' => '_blank')).'</td>
								<td>'.CHtml::encode($cam->owner->nick).'</td>
								<td>'.CHtml::encode($cam->cam->desc).'</td>
								<td>'.($cam->show ? Yii::t('cams', 'Show') : Yii::t('cams', 'Don\'t show')).'</td>
								<td>'.CHtml::link(Yii::t('cams', 'Delete'), $this->createUrl('cams/delete', array('id' => $cam->id, 'type' => 'share')), array('class' => 'btn btn-danger')).'</td>
								</tr>';
							}
							?>
							<tr>
								<td colspan="5"><?php echo Yii::t('cams', 'Mass actions: '); ?></td>
								<td><?php echo CHtml::submitButton(Yii::t('cams', 'Show/Don\'t show'), array('name' => 'show', 'class' => 'btn btn-primary')); ?></td>
								<td><?php echo CHtml::submitButton(Yii::t('cams', 'Delete'), array('name' => 'del', 'class' => 'btn btn-danger')); ?></td>
							</tr>
							<?php echo CHtml::endForm(); ?>
						</tbody>
					</table>
					<?php
				} else {
					echo Yii::t('cams', 'List cams shared for me is empty.<br/>');
				}
				?>
			</div>
		</div>
	</div>
	<div class="col-sm-12">
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title"><?php echo Yii::t('cams', 'Public cams'); ?></h3>
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
								<th><?php echo Yii::t('cams', 'Name'); ?></th>
								<th><?php echo Yii::t('cams', 'Owner'); ?></th>
								<th><?php echo Yii::t('cams', 'Description'); ?></th>
								<th><?php echo Yii::t('cams', 'Show?'); ?></th>
							</tr>
						</thead>
						<tbody>
							<?php
							echo CHtml::beginForm($this->createUrl('cams/manage'), "post");
							foreach ($myPublicCams as $key => $cam) {
								if(isset($cam->cam_id)) {
									$cam = $cam->cam;
									$cam->show = $myPublicCams[$key]->show;
								} else {
									$cam->show = 1;
								}
								echo '<tr>
								<td>'.CHtml::activeCheckBox($form, 'pcam_'.$cam->getSessionId()).'</td>
								<td>'.($key + 1).'</td>
								<td>'.CHtml::link(CHtml::encode($cam->name), $this->createUrl('cams/archive', array('id' => $cam->getSessionId(), 'full' => '1')), array('target' => '_blank')).'</td>
								<td>'.CHtml::encode($cam->owner->nick).'</td>
								<td>'.CHtml::encode($cam->desc).'</td>
								<td>'.($cam->show ? Yii::t('cams', 'Show') : Yii::t('cams', 'Don\'t show')).'</td>
								</tr>';
							}
							?>
							<tr>
								<td colspan="5"><?php echo Yii::t('cams', 'Mass actions: '); ?></td>
								<td><?php echo CHtml::submitButton(Yii::t('cams', 'Show/Don\'t show'), array('name' => 'show', 'class' => 'btn btn-primary')); ?></td>
							</tr>
							<?php echo CHtml::endForm(); ?>
						</tbody>
					</table>
					<?php
				} else {
					echo Yii::t('cams', 'Public cams list is empty.<br/>');
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