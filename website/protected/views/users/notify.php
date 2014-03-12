<?php
/* @var $this UsersController */
?>
<div class="col-sm-12">
	<div class="panel panel-default">
		<div class="panel-heading">
			<h3 class="panel-title"><?php echo Yii::t('user', 'New notifications'); ?></h3>
		</div>
		<div class="panel-body">
			<?php
			$creators = array('0' => 'system');
			if($new) {
				?>
				<table class="table table-striped">
				<thead>
					<tr>
					<td>#</td>
					<td><?php echo Yii::t('user', 'From'); ?></td>
					<td><?php echo Yii::t('user', 'Message'); ?></td>
					</tr>
				</thead>
				<tbody>
					<?php
					foreach ($new as $key => $value) {
						if(!isset($creators[$value->creator_id])) {
							$user = Users::model()->findByPK($value->creator_id);
							$creators[$value->creator_id] = $user->nick;
						}
						?>
						<tr>
							<td><?php echo $key+1; ?></td>
							<td><?php echo $creators[$value->creator_id]; ?></td>
							<td><?php echo $value->msg; ?></td>
						</tr>
						<?php	
					}
					?>
					</tbody>
				</table>
				<?php
			} else {
				echo Yii::t('user', 'There is no new notifications');
			}
			?>
		</div>
	</div>
</div>
<div class="col-sm-12">
	<div class="panel panel-default">
		<div class="panel-heading">
			<h3 class="panel-title"><?php echo Yii::t('user', 'Old notifications'); ?></h3>
		</div>
		<div class="panel-body">
			<?php
			if($old) {
				?>
				<table class="table table-striped">
				<thead>
					<tr>
					<td>#</td>
					<td><?php echo Yii::t('user', 'From'); ?></td>
					<td><?php echo Yii::t('user', 'Message'); ?></td>
					</tr>
				</thead>
				<tbody>
					<?php
					foreach ($old as $key => $value) {
						if(!isset($creators[$value->creator_id])) {
							$user = Users::model()->findByPK($value->creator_id);
							$creators[$value->creator_id] = $user->nick;
						}
						?>
						<tr>
							<td><?php echo $key+1; ?></td>
							<td><?php echo $creators[$value->creator_id]; ?></td>
							<td><?php echo $value->msg; ?></td>
						</tr>
						<?php	
					}
					?>
					</tbody>
				</table>
				<?php
			} else {
				echo Yii::t('user', 'There is no old notifications');
			}
			?>
		</div>
	</div>
</div>
