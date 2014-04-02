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
							<th>#</th>
							<th><?php echo Yii::t('user', 'From'); ?></th>
							<th><?php echo Yii::t('user', 'Message'); ?></th>
							<th><?php echo Yii::t('user', 'Action'); ?></th>
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
								<td>
									<div class="btn-toolbar">
										<?php
										if($value->shared_id) {
											echo CHtml::link(Yii::t('user', 'Approve'), $this->createUrl('users/notifications', array('id' => $value->id, 'action' => 'approve')), array('class' => 'btn btn-success'));
											echo CHtml::link(Yii::t('user', 'Disapprove'), $this->createUrl('users/notifications', array('id' => $value->id, 'action' => 'disapprove')), array('class' => 'btn btn-danger'));
										} else {
											echo Yii::t('user', 'There is no actions');
										}
										?>
									</div>
								</td>
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
							<th>#</th>
							<th><?php echo Yii::t('user', 'From'); ?></th>
							<th><?php echo Yii::t('user', 'Message'); ?></th>
							<th><?php echo Yii::t('user', 'Action'); ?></th>
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
								<td>
									<div class="btn-toolbar">
										<?php
										if($value->status == 2) {
											echo CHtml::link(Yii::t('user', 'Accepted'), $this->createUrl('#'), array('class' => 'btn btn-success disabled'));
										} elseif($value->status == 3) {
											echo CHtml::link(Yii::t('user', 'Declined'), $this->createUrl('#'), array('class' => 'btn btn-danger disabled'));
										} elseif($value->shared_id) {
											echo CHtml::link(Yii::t('user', 'Accept'), $this->createUrl('users/notifications', array('id' => $value->id, 'action' => 'approve')), array('class' => 'btn btn-success'));
											echo CHtml::link(Yii::t('user', 'Decline'), $this->createUrl('users/notifications', array('id' => $value->id, 'action' => 'disapprove')), array('class' => 'btn btn-danger'));
										} else {
											echo Yii::t('user', 'There is no actions');
										}
										?>
									</div>
								</td>
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
