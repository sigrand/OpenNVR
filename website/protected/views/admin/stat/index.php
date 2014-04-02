<div class="col-sm-12">
    <div class="panel panel-default">
        <div class="panel-heading">
            <ul class="nav nav-tabs">
                <li <?php echo $type == 'disk' ? 'class="active"' : ''; ?>>
                    <?php echo CHtml::link(Yii::t('admin', 'Hard drive sections'), $this->createUrl('admin/stat', array('type' => 'disk', 'id' => $id))); ?>
                </li>
                <li <?php echo $type == 'load' ? 'class="active"' : ''; ?>>
                    <?php echo CHtml::link(Yii::t('admin', 'Resource load'), $this->createUrl('admin/stat', array('type' => 'load', 'id' => $id))); ?>
                </li>
                <li <?php echo $type == 'rtmp' ? 'class="active"' : ''; ?>>
                    <?php echo CHtml::link(Yii::t('admin', 'RTMP stat'), $this->createUrl('admin/stat', array('type' => 'rtmp', 'id' => $id))); ?>
                </li>
            </ul>
        </div>
        <div class="panel-body">
            <?php
            if (empty($stat)) {
                echo Yii::t('admin', 'Stat not available');
            } else {
                switch ($type) {
                    case 'disk':
                    echo '<table class="table">
                    <thead>
                    <th>'.Yii::t('admin', 'Section').'</th>
                    <th>'.Yii::t('admin', 'Size').'</th>
                    <th>'.Yii::t('admin', 'Free').'</th>
                    <th>'.Yii::t('admin', 'Used').'</th>
                    </thead>
                    <tbody>
                    ';
                    foreach ($stat as $s) {
                        echo '
                        <tr>
                        <td>'.$s['disk name'].'</td>
                        <td>'.$s['disk info']['all size'].'</td>
                        <td>'.$s['disk info']['free size'].'</td>
                        <td>'.$s['disk info']['used size'].'</td>
                        </tr>
                        ';
                    }
                    echo '</tbody></table><br/>';
                    break;   

                    case 'rtmp':
                    echo $stat;
                    break;
                    
                    case 'load':
                    Yii::import('ext.charts.*');
                    $chart = new Highchart();
                    $chart->tooltip->formatter = new HighchartJsExpr("function() { return this.y + ' : ' + this.x; }");
                    $chart->printScripts();
                    $i = 0;
                    foreach ($stat as $key => $value) {
                        if ($key == 'time') {
                            continue;
                        }
                        echo '<div id="container' . $i . '"></div><br/>';
                        $chart->chart = array(
                            'renderTo' => 'container' . $i,
                            'type' => 'line',
                            'marginRight' => 130,
                            'marginBottom' => 50,
                            'spacingBottom' => 30,
                            );

                        $chart->title = array(
                            'text' => $key,
                            'x' => -20
                            );

                        $chart->xAxis = array(
                            'categories' => $stat['time'],
                            'offset' => 20,
                            );

                        $chart->yAxis = array(
                            'title' => array(
                                'text' => Yii::t('admin', 'Value')
                                ),
                            'min' => 0,
                            'plotLines' => array(
                                array(
                                    'value' => 0,
                                    'width' => 1,
                                    'color' => '#808080'
                                    )
                                )
                            );
                        $chart->legend = array(
                            'layout' => 'vertical',
                            'align' => 'right',
                            'verticalAlign' => 'top',
                            'x' => -10,
                            'y' => 100,
                            'borderWidth' => 0
                            );
                        $chart->series = array();
                        $chart->series[] = array(
                            'name' => 'min',
                            'data' => $value['min']
                            );
                        $chart->series[] = array(
                            'name' => 'max',
                            'data' => $value['max']
                            );
                        $chart->series[] = array(
                            'name' => 'avg',
                            'data' => $value['avg']
                            );

                        echo '<script type="text/javascript">' . $chart->render("chart") . '</script>';
                        $i++;
                    }
                    break;
                    default:
                        # code...
                    break;
                }
            }
            ?>
        </div>
    </div>
</div>