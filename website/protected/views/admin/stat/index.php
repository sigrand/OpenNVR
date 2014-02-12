<div class="col-sm-12">
    <div class="panel panel-default">
        <div class="panel-heading">
            <ul class="nav nav-tabs">
                <li <?php echo $type == 'disk' ? 'class="active"' : ''; ?>>
                    <?php echo CHtml::link(Yii::t('admin', 'Разделы жесткого диска'), $this->createUrl('admin/stat', array('type' => 'disk'))); ?>
                </li>
                <li <?php echo $type == 'load' ? 'class="active"' : ''; ?>>
                    <?php echo CHtml::link(Yii::t('admin', 'Нагрузка ресурсов'), $this->createUrl('admin/stat', array('type' => 'load'))); ?>
                </li>
                <li <?php echo $type == 'rtmp' ? 'class="active"' : ''; ?>>
                    <?php echo CHtml::link(Yii::t('admin', 'RTMP Статистика'), $this->createUrl('admin/stat', array('type' => 'rtmp'))); ?>
                </li>
            </ul>
        </div>
        <div class="panel-body">
            <?php
            if (empty($stat)) {
                echo Yii::t('admin', 'Статистика недоступна');
            } else {
                switch ($type) {
                    case 'disk':
                    echo '<table class="table">
                    <thead>
                    <th>'.Yii::t('admin', 'Раздел').'</th>
                    <th>'.Yii::t('admin', 'Емкость').'</th>
                    <th>'.Yii::t('admin', 'Свободно').'</th>
                    <th>'.Yii::t('admin', 'Использованно').'</th>
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
                                'text' => Yii::t('admin', 'Значение')
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