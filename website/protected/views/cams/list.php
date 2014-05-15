<div class="col-sm-12">
	<?php
	if(Yii::app()->user->hasFlash('notify')) {
		$notify = Yii::app()->user->getFlash('notify');
		?>
		<div class="alert alert-<?php echo $notify['type']; ?>"><?php echo $notify['message']; ?>.</div>
		<?php } ?>
		<div class="panel panel-default">
			<div class="panel-heading">
				<ul class="nav nav-tabs">
					<li class='active' id='my_cams'>
						<?php echo CHtml::link(Yii::t('cams', 'My cams'), '#'); ?>
					</li>
					<li class='' id='shared_cams'>
						<?php echo CHtml::link(Yii::t('cams', 'Shared cams'), '#'); ?>
					</li>
					<li class='' id='public_cams'>
						<?php echo CHtml::link(Yii::t('cams', 'Public cams'), '#'); ?>
					</li>
				</ul>
			</div>
			<div class="panel-body" id='my_cams_tab'>
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
								<td colspan="<?php echo (Yii::app()->user->permissions == 2) || (Yii::app()->user->permissions == 3) ? 4 : 5 ?>"><?php echo Yii::t('cams', 'Mass actions: '); ?></td>
								<?php echo (Yii::app()->user->permissions == 2) || (Yii::app()->user->permissions == 3) ? '<td>'.CHtml::submitButton(Yii::t('cams', 'assign'), array('name' => 'assign', 'class' => 'btn btn-success')).'</td>' : ''; ?>
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
			<div class="panel-body" id='shared_cams_tab' style='display:none'>
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
								<td colspan="3"><?php echo Yii::t('cams', 'Mass actions: '); ?></td>
								<?php echo (Yii::app()->user->permissions == 2) || (Yii::app()->user->permissions == 3) ? '<td>'.CHtml::submitButton(Yii::t('cams', 'assign'), array('name' => 'doubleAssign', 'class' => 'btn btn-success')).'</td>' : ''; ?>
								<td><?php echo CHtml::submitButton(Yii::t('cams', 'Share'), array('name' => 'doubleShare', 'class' => 'btn btn-success')); ?></td>
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
			<div class="panel-body" id='public_cams_tab' style='display:none'>
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
		$('.nav-tabs li').click(function(){
			$(".nav-tabs li").removeClass("active");
			$("#"+this.id).addClass("active");
			$(".panel-body").hide();
			$("#"+this.id+"_tab").show();
		});
	});
	</script>