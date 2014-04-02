<?php
/* @var $this AdminController */
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
				<h3 class="panel-title"><?php echo Yii::t('admin', 'Admins'); ?></h3>
			</div>
			<div class="panel-body">
				<?php
				if(!empty($admins)) {
					?>
					<table class="table table-striped">
						<thead>
							<th><?php echo CHtml::activeCheckBox($form, "checkAllA", array ("class" => "checkAllA")); ?></th>
							<th>#</th>
							<th><?php echo Yii::t('admin', 'nick'); ?></th>
							<th><?php echo Yii::t('admin', 'email'); ?></th>
						</thead>
						<tbody>
							<?php
							echo CHtml::beginForm($this->createUrl('admin/users'), "post");
							foreach ($admins as $key => $user) {
								echo '<tr>
								<td>'.CHtml::activeCheckBox($form, 'auser_'.$user->id).'</td>
								<td>'.($key+1).'</td>
								<td>'.CHtml::encode($user->nick).'</td>
								<td>'.CHtml::encode($user->email).'</td>
								</tr>';
							}
							?>
							<tr>
								<td colspan="2"></td>
								<td><?php echo CHtml::submitButton(Yii::t('admin', 'LevelDown'), array('name' => 'dismiss', 'class' => 'btn btn-primary')); ?></td>
								<td><?php echo CHtml::submitButton(Yii::t('admin', 'Ban'), array('name' => 'ban', 'class' => 'btn btn-danger')); ?></td>
							</tr>
							<?php echo CHtml::endForm(); ?>
						</tbody>
					</table>
					<?php
				} else {
					echo Yii::t('admin', 'Admins list is empty.<br/>');
				}
				?>
			</div>
		</div>
	</div>
	<div class="col-sm-12">
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title"><?php echo Yii::t('admin', 'Operators'); ?></h3>
			</div>
			<div class="panel-body">
				<?php
				if(!empty($operators)) {
					?>
					<table class="table table-striped">
						<thead>
							<th><?php echo CHtml::activeCheckBox($form, "checkAllO", array ("class" => "checkAllO")); ?></th>
							<th>#</th>
							<th><?php echo Yii::t('admin', 'Nick'); ?></th>
							<th><?php echo Yii::t('admin', 'Email'); ?></th>
						</thead>
						<tbody>
							<?php
							echo CHtml::beginForm($this->createUrl('admin/users'), "post");
							foreach ($operators as $key => $user) {
								echo '<tr>
								<td>'.CHtml::activeCheckBox($form, 'ouser_'.$user->id).'</td>
								<td>'.($key+1).'</td>
								<td>'.CHtml::encode($user->nick).'</td>
								<td>'.CHtml::encode($user->email).'</td>
								</tr>';
							}
							?>
							<tr>
								<td colspan="1"></td>
								<td><?php echo CHtml::submitButton('LevelUp', array('name' => 'levelup', 'class' => 'btn btn-success')); ?></td>
								<td><?php echo CHtml::submitButton('LevelDown', array('name' => 'dismiss', 'class' => 'btn btn-primary')); ?></td>
								<td><?php echo CHtml::submitButton('Ban', array('name' => 'ban', 'class' => 'btn btn-danger')); ?></td>
							</tr>
							<?php echo CHtml::endForm(); ?>
						</tbody>
					</table>
					<?php
				} else {
					echo Yii::t('admin', 'Operators list is empty<br/>');
				}
				?>
			</div>
		</div>
	</div>
	<div class="col-sm-12">
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title"><?php echo Yii::t('admin', 'Viewers'); ?></h3>
			</div>
			<div class="panel-body">
				<?php
				if(!empty($viewers)) {
					?>
					<table class="table table-striped">
						<thead>
							<th><?php echo CHtml::activeCheckBox($form, "checkAllV", array ("class" => "checkAllV")); ?></th>
							<th>#</th>
							<th>Ник</th>
							<th>Почта</th>
						</thead>
						<tbody>
							<?php
							echo CHtml::beginForm($this->createUrl('admin/users'), "post");
							foreach ($viewers as $key => $user) {
								echo '<tr>
								<td>'.CHtml::activeCheckBox($form, 'vuser_'.$user->id).'</td>
								<td>'.($key+1).'</td>
								<td>'.CHtml::encode($user->nick).'</td>
								<td>'.CHtml::encode($user->email).'</td>
								</tr>';
							}
							?>
							<tr>
								<td colspan="2"></td>
								<td><?php echo CHtml::submitButton('LevelUp', array('name' => 'levelup', 'class' => 'btn btn-success')); ?></td>
								<td><?php echo CHtml::submitButton('Ban', array('name' => 'ban', 'class' => 'btn btn-danger')); ?></td>
							</tr>
							<?php echo CHtml::endForm(); ?>
						</tbody>
					</table>
					<?php
				} else {
					echo Yii::t('admin', 'Viewers list is empty<br/>');
				}
				?>
			</div>
		</div>
	</div>
	<div class="col-sm-12">
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title"><?php echo Yii::t('admin', 'Banned'); ?></h3>
			</div>
			<div class="panel-body">
				<?php
				if(!empty($banned)) {
					?>
					<table class="table table-striped">
						<thead>
							<th><?php echo CHtml::activeCheckBox($form, "checkAllZ", array ("class" => "checkAllZ")); ?></th>
							<th>#</th>
							<th><?php echo Yii::t('admin', 'nick'); ?></th>
							<th><?php echo Yii::t('admin', 'email'); ?></th>
						</thead>
						<tbody>
							<?php
							echo CHtml::beginForm($this->createUrl('admin/users'), "post");
							foreach ($banned as $key => $user) {
								echo '<tr>
								<td>'.CHtml::activeCheckBox($form, 'zuser_'.$user->id).'</td>
								<td>'.($key+1).'</td>
								<td>'.CHtml::encode($user->nick).'</td>
								<td>'.CHtml::encode($user->email).'</td>
								</tr>';
							}
							?>
							<tr>
								<td colspan="3"></td>
								<td><?php echo CHtml::submitButton(Yii::t('admin', 'Unban'), array('name' => 'unban', 'class' => 'btn btn-primary')); ?></td>
							</tr>
							<?php echo CHtml::endForm(); ?>
						</tbody>
					</table>
					<?php
				} else {
					echo Yii::t('admin', 'Banned list is empty<br/>');
				}
				?>
			</div>
		</div>
	</div>
	<div class="col-sm-12">
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title"><?php echo Yii::t('admin', 'All others'); ?></h3>
			</div>
			<div class="panel-body">
				<?php
				if(!empty($all)) {
					?>
					<table class="table table-striped">
						<thead>
							<th><?php echo CHtml::activeCheckBox($form, "checkAll", array ("class" => "checkAll")); ?></th>
							<th>#</th>
							<th><?php echo Yii::t('admin', 'Nick'); ?></th>
							<th><?php echo Yii::t('admin', 'Email'); ?></th>
							<th><?php echo Yii::t('admin', 'Status'); ?></th>
						</thead>
						<tbody>
							<?php
							echo CHtml::beginForm($this->createUrl('admin/users'), "post");
							foreach ($all as $key => $user) {
								echo '<tr>
								<td>'.CHtml::activeCheckBox($form, 'user_'.$user->id).'</td>
								<td>'.($key+1).'</td>
								<td>'.CHtml::encode($user->nick).'</td>
								<td>'.CHtml::encode($user->email).'</td>
								<td>'.($user->status == 1 ? Yii::t('admin', 'Activated') : Yii::t('admin', 'Not activated')).'</td>
								</tr>';
							}
							?>
							<tr>
								<td colspan="2">
									<?php
									$this->widget('CLinkPager', array(
										'pages' => $pages,
										'header' => Yii::t('admin', 'Go to page: '),
										'nextPageLabel' => Yii::t('admin', 'next'),
										'prevPageLabel' => Yii::t('admin', 'back'),
										));
										?>
									</td>
									<td><?php echo CHtml::submitButton(Yii::t('admin', 'Activate'), array('name' => 'active', 'class' => 'btn btn-primary')); ?></td>
									<td><?php echo CHtml::submitButton(Yii::t('admin', 'Ban'), array('name' => 'ban', 'class' => 'btn btn-danger')); ?></td>
								</tr>
								<?php echo CHtml::endForm(); ?>
							</tbody>
						</table>
						<?php
					} else {
						echo Yii::t('admin', 'Users list is empty<br/>');
					}
					?>
				</div>
			</div>
		</div>
		<script>
		$(document).ready(function(){
			$(".checkAll").click(function(){
				$('input[id*="UsersForm_user_"]').not(this).prop('checked', this.checked);
			});
			$(".checkAllA").click(function(){
				$('input[id*="UsersForm_auser_"]').not(this).prop('checked', this.checked);
			});		
			$(".checkAllO").click(function(){
				$('input[id*="UsersForm_ouser_"]').not(this).prop('checked', this.checked);
			});
			$(".checkAllZ").click(function(){
				$('input[id*="UsersForm_zuser_"]').not(this).prop('checked', this.checked);
			});			
			$(".checkAllV").click(function(){
				$('input[id*="UsersForm_vuser_"]').not(this).prop('checked', this.checked);
			});
		});
		</script>