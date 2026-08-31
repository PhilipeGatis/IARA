import { createContext, useContext, useState, useCallback, type ReactNode } from 'react';
import { api } from './api';

export type Lang = 'pt' | 'en' | 'ja';

const translations = {
    // ---- Navigation ----
    'nav.home': { pt: 'Início', en: 'Home', ja: 'ホーム' },
    'nav.tpa': { pt: 'TPA', en: 'TPA', ja: 'TPA' },
    'nav.ferts': { pt: 'Ferts', en: 'Ferts', ja: '肥料' },
    'nav.config': { pt: 'Config', en: 'Config', ja: '設定' },

    // ---- Emergency ----
    'emergency.banner': { pt: '⚠️ EMERGÊNCIA — Sensor detectou risco de transbordamento! Parando bombas imediatamente.', en: '⚠️ EMERGENCY — Sensor detected overflow risk! Stopping pumps immediately.', ja: '⚠️ 緊急事態 — センサーがオーバーフローリスクを検出！ポンプを即時停止。' },

    'maintenance.banner': { pt: '🔧 Modo manutenção ativo — TPA e fertilização automáticas estão pausadas.', en: '🔧 Maintenance mode on — automatic TPA and dosing are paused.', ja: '🔧 メンテナンスモード作動中 — 自動TPAと施肥は一時停止しています。' },

    // ---- HomeTab ----
    'home.sensors': { pt: 'Sensores e Segurança', en: 'Sensors & Safety', ja: 'センサーと安全' },
    'home.waterLevel': { pt: 'Nível de Água', en: 'Water Level', ja: '水位' },
    'home.float': { pt: 'Boia do Reservatório', en: 'Float Switch', ja: 'フロートスイッチ' },
    'home.floatOn': { pt: 'NO NÍVEL', en: 'AT LEVEL', ja: 'レベル内' },
    'home.floatOff': { pt: 'VAZIO', en: 'EMPTY', ja: '空' },
    'home.feed': { pt: 'Alimentar', en: 'Feed', ja: '給餌' },
    'home.feedHint': { pt: 'Desliga o filtro por {n} min', en: 'Switches the filter off for {n} min', ja: 'フィルターを{n}分停止' },
    'home.feedActive': { pt: 'Filtro desligado · {t}', en: 'Filter off · {t}', ja: 'フィルター停止中 · {t}' },
    'home.feedRestore': { pt: 'Tocar para religar agora', en: 'Tap to switch it back on', ja: 'タップで今すぐ再開' },
    'home.feedBusy': { pt: 'Indisponível durante uma TPA', en: 'Unavailable during a water change', ja: '換水中は利用できません' },
    'home.canister': { pt: 'Filtro Canister', en: 'Canister Filter', ja: 'キャニスターフィルター' },
    'home.canisterOn': { pt: 'LIGADA', en: 'ON', ja: 'オン' },
    'home.canisterOff': { pt: 'DESL.', en: 'OFF', ja: 'オフ' },
    'home.systemState': { pt: 'Estado do Sistema', en: 'System State', ja: 'システム状態' },
    'home.tpaState': { pt: 'Estado TPA', en: 'TPA State', ja: 'TPA状態' },
    'home.maintenance': { pt: 'Modo Manutenção', en: 'Maintenance Mode', ja: 'メンテナンスモード' },
    'home.maintActive': { pt: 'ATIVO', en: 'ACTIVE', ja: '有効' },
    'home.maintInactive': { pt: 'INATIVO', en: 'INACTIVE', ja: '無効' },
    'home.tpaSchedule': { pt: 'Agendamento TPA', en: 'TPA Schedule', ja: 'TPAスケジュール' },
    'home.configIncomplete': { pt: '⚠ Configuração incompleta — TPA desativada', en: '⚠ Incomplete configuration — TPA disabled', ja: '⚠ 設定不完全 — TPA無効' },
    'home.volume': { pt: 'Volume', en: 'Volume', ja: '容量' },
    'home.interval': { pt: 'Intervalo', en: 'Interval', ja: '間隔' },
    'home.everyDays': { pt: 'a cada {n} dia{s}', en: 'every {n} day{s}', ja: '{n}日ごと' },
    'home.time': { pt: 'Horário', en: 'Time', ja: '時刻' },
    'home.lastRun': { pt: 'Última execução', en: 'Last run', ja: '前回実行' },
    'home.never': { pt: 'nunca', en: 'never', ja: '未実行' },
    'home.nextTpa': { pt: 'Próxima TPA', en: 'Next TPA', ja: '次回TPA' },
    'home.today': { pt: 'HOJE', en: 'TODAY', ja: '今日' },
    'home.tomorrow': { pt: 'amanhã', en: 'tomorrow', ja: '明日' },
    'home.inDays': { pt: 'em {n} dias', en: 'in {n} days', ja: '{n}日後' },
    'home.tpaMissed': { pt: 'A TPA de {d} não foi concluída. O horário acima é a próxima tentativa.', en: 'The {d} water change did not complete. The time above is the next attempt.', ja: '{d}のTPAは完了しませんでした。上記は次回の試行です。' },
    'home.noSchedule': { pt: 'Nenhum agendamento configurado.', en: 'No schedule configured.', ja: 'スケジュール未設定。' },
    'home.stockBars': { pt: 'Estoque de Fertilizantes', en: 'Fertilizer Stock', ja: '肥料在庫' },
    'home.fertTable': { pt: 'Tabela de Fertilizantes', en: 'Fertilizer Table', ja: '肥料テーブル' },
    'home.inMl': { pt: 'Em mL', en: 'In mL', ja: 'mL単位' },
    'home.channel': { pt: 'Canal', en: 'Channel', ja: 'チャンネル' },
    'home.hour': { pt: 'Hora', en: 'Hour', ja: '時間' },
    'home.waiting': { pt: 'Aguardando dados...', en: 'Waiting for data...', ja: 'データ待ち...' },
    'home.noActiveSchedule': { pt: 'Nenhum agendamento ativo na semana.', en: 'No active schedule this week.', ja: '今週のスケジュールなし。' },
    'home.shortDays': { pt: 'D,S,T,Q,Q,S,S', en: 'S,M,T,W,T,F,S', ja: '日,月,火,水,木,金,土' },

    // ---- TPATab ----
    'tpa.start': { pt: 'Iniciar TPA', en: 'Start TPA', ja: 'TPA開始' },
    'tpa.abort': { pt: 'Abortar TPA', en: 'Abort TPA', ja: 'TPA中止' },
    'tpa.auto': { pt: 'TPA Automática', en: 'Auto TPA', ja: '自動TPA' },
    'tpa.autoEnabled': { pt: 'Habilitar TPA Automática', en: 'Enable Auto TPA', ja: '自動TPAを有効にする' },
    'tpa.autoEnabledHint': { pt: 'Se desmarcado, a TPA não rodará automaticamente, apenas via comando manual.', en: 'If unchecked, TPA will not run automatically, only manually.', ja: 'チェックを外すと、TPAは自動実行されず、手動コマンドのみで実行されます。' },
    'tpa.frequency': { pt: 'Frequência (Dias)', en: 'Frequency (Days)', ja: '頻度（日数）' },
    'tpa.cancel': { pt: 'Cancelar TPA', en: 'Cancel TPA', ja: 'TPAをキャンセル' },
    'tpa.freqHint': { pt: 'Coloque 0 para desativar. (Ex: 7 = Semanal, 15 = Quinzenal)', en: 'Set 0 to disable. (e.g. 7 = Weekly, 15 = Biweekly)', ja: '0で無効。（例: 7 = 毎週, 15 = 隔週）' },
    'tpa.disabled': { pt: '0 = Desativado', en: '0 = Disabled', ja: '0 = 無効' },
    'tpa.volumePct': { pt: 'Volume da TPA (%)', en: 'TPA Volume (%)', ja: 'TPA容量（%）' },
    'tpa.ofTotal': { pt: 'de {v} L totais', en: 'of {v} L total', ja: '全{v} L中' },
    'tpa.configDimHint': { pt: 'Configure as dimensões do aquário na aba Config', en: 'Set aquarium dimensions in Config tab', ja: 'Configタブで水槽サイズを設定' },
    'tpa.hour': { pt: 'Hora', en: 'Hour', ja: '時' },
    'tpa.minute': { pt: 'Minuto', en: 'Minute', ja: '分' },
    'tpa.scheduled': { pt: 'Agendado para:', en: 'Scheduled for:', ja: '予定:' },
    'tpa.disabledLabel': { pt: 'Desativado', en: 'Disabled', ja: '無効' },
    'tpa.saveSchedule': { pt: 'Salvar Horário', en: 'Save Schedule', ja: 'スケジュール保存' },
    'tpa.lastRun': { pt: 'Última TPA registrada', en: 'Last recorded water change', ja: '最後に記録された水換え' },
    'tpa.lastRunNever': { pt: 'Nenhuma — a próxima roda no horário agendado', en: 'None — the next one runs at the scheduled time', ja: 'なし — 次回は予定時刻に実行' },
    'tpa.resetLastRun': { pt: 'Zerar', en: 'Clear', ja: 'クリア' },
    'tpa.markLastRun': { pt: 'Foi feita agora', en: 'Done just now', ja: '今実施した' },
    'tpa.lastRunHint': { pt: 'Zerar faz a próxima TPA acontecer no próximo horário agendado. "Foi feita agora" reinicia a contagem a partir deste instante.', en: 'Clearing makes the next water change due at the next scheduled time. "Done just now" restarts the interval from this moment.', ja: 'クリアすると次回の水換えは次の予定時刻に実行されます。「今実施した」はこの時点から間隔を数え直します。' },
    'tpa.incompleteConfig': { pt: '⚠ Configuração Incompleta', en: '⚠ Incomplete Configuration', ja: '⚠ 設定不完全' },
    'tpa.incompleteMsg': { pt: 'A TPA não será executada até que todos os campos obrigatórios sejam preenchidos na aba', en: 'TPA will not run until all required fields are filled in the', ja: '必須項目がすべて入力されるまでTPAは実行されません。' },
    'tpa.dimMissing': { pt: 'Dimensões do aquário (A×C×L)', en: 'Aquarium dimensions (H×L×W)', ja: '水槽サイズ（高×長×幅）' },
    'tpa.reservoirMissing': { pt: 'Volume do reservatório', en: 'Reservoir volume', ja: 'リザーバーの容量' },
    'tpa.pctMissing': { pt: 'Volume da TPA (%)', en: 'TPA volume (%)', ja: '換水量（%）' },
    'tpa.sensorMissing': { pt: 'Calibração do sensor ultrassônico', en: 'Ultrasonic sensor calibration', ja: '超音波センサーのキャリブレーション' },
    'tpa.drainPumpMissing': { pt: 'Calibração da bomba de drenagem', en: 'Drain pump calibration', ja: '排水ポンプのキャリブレーション' },
    'tpa.refillPumpMissing': { pt: 'Calibração da bomba de Recalque', en: 'Refill pump calibration', ja: '給水ポンプのキャリブレーション' },
    'tpa.safetyMlMissing': { pt: 'Margem de segurança do reservatório', en: 'Reservoir safety margin', ja: 'リザーバーの安全マージン' },
    'tpa.reservoir': { pt: 'Reservatório e Prime', en: 'Reservoir & Prime', ja: 'リザーバーとプライム' },
    'tpa.safetyMargin': { pt: 'Margem de Segurança (Bomba TPA)', en: 'Safety Margin (TPA Pump)', ja: '安全マージン（TPAポンプ）' },
    'tpa.safetyHint': { pt: 'Deixe sempre X ml de água no fundo do reservatório', en: 'Always leave X ml of water in the reservoir', ja: 'リザーバーの底に常にX mlの水を残す' },
    'tpa.drainPumpShort': { pt: 'Bomba de drenagem', en: 'Drain pump', ja: '排水ポンプ' },
    'tpa.refillPumpShort': { pt: 'Bomba de recalque', en: 'Refill pump', ja: '給水ポンプ' },
    'tpa.fillReservoir': { pt: 'Encher Reservatório', en: 'Fill Reservoir', ja: 'リザーバーを満水にする' },
    'tpa.stopFill': { pt: 'Parar Enchimento', en: 'Stop Fill', ja: '給水停止' },
    'tpa.goalHint': { pt: 'Meta em % do aquário. Máximo {max}% — o reservatório tem {res} L e o aquário {aq} L.', en: 'Goal as % of the aquarium. Maximum {max}% — the reservoir holds {res} L and the aquarium {aq} L.', ja: '目標は水槽に対する%。最大{max}% — リザーバー{res} L、水槽{aq} L。' },
    'tpa.goalHintNoConfig': { pt: 'Configure o volume do aquário e do reservatório para liberar as metas.', en: 'Set the aquarium and reservoir volumes to enable goals.', ja: '目標を有効にするには、水槽とリザーバーの容量を設定してください。' },
    'tpa.pumpControl': { pt: 'Controle Manual (Ligar/Desligar)', en: 'Manual Control (On/Off)', ja: '手動制御 (オン/オフ)' },
    'tpa.primeDose': { pt: 'Dose de Prime no Reservatório', en: 'Prime Dose in Reservoir', ja: 'リザーバーのプライム投与量' },
    'tpa.fillTimeout': { pt: 'Tempo máximo de enchimento (min)', en: 'Fill window (min)', ja: '給水の最大時間（分）' },
    'tpa.fillTimeoutHint': { pt: 'Não é quanto o enchimento demora — é o ponto além do qual a boia claramente não acionou e deve ser considerada quebrada. Deixe folgado. 0 desativa.', en: 'Not how long a fill takes — the point past which the float plainly did not close and should be treated as failed. Keep it generous. 0 disables it.', ja: '給水にかかる時間ではなく、フロートが明らかに動作せず故障とみなすべき時点です。余裕を持たせてください。0で無効。' },
    'tpa.mechFloat': { pt: 'Reservatório tem boia mecânica', en: 'Reservoir has a mechanical float valve', ja: 'リザーバーに機械式ボールタップあり' },
    'tpa.mechFloatHint': { pt: 'A válvula corta a água no nível cheio sozinha. Com isso, esgotar o tempo de enchimento deixa de ser erro — mas o sistema perde a capacidade de perceber um enchimento que falhou por falta de pressão.', en: 'The valve shuts the water at the full level on its own. Running out the fill window then stops being an error — but the system loses the ability to notice a fill that failed for want of mains pressure.', ja: 'バルブが満水位で自動的に給水を止めます。給水時間の超過はエラーではなくなりますが、水圧不足で給水が失敗した場合にそれを検知できなくなります。' },
    'tpa.primeEnabled': { pt: 'Habilitar Prime na TPA (Canal 5)', en: 'Enable Prime in TPA (Channel 5)', ja: 'TPAでプライムを有効にする（チャンネル5）' },
    'tpa.primeEnabledHint': { pt: 'Se desabilitado, o Canal 5 pode ser usado como um fertilizante genérico.', en: 'If disabled, Channel 5 can be used as a generic fertilizer.', ja: '無効にすると、チャンネル5は一般的な肥料として使用できます。' },
    'tpa.configInConfigTab': { pt: 'Configure na aba Config', en: 'Set in Config tab', ja: 'Configタブで設定' },
    'tpa.autoCalc': { pt: '(calculada automaticamente: reservatório × proporção)', en: '(auto-calculated: reservoir × ratio)', ja: '（自動計算: リザーバー × 比率）' },
    'tpa.saveConfig': { pt: 'Salvar Configuração', en: 'Save Config', ja: '設定保存' },
    'tpa.flowRates': { pt: 'Vazões das Bombas', en: 'Pump Flow Rates', ja: 'ポンプ流量' },
    'tpa.drainPump': { pt: 'Bomba de Drenagem (Esvaziamento)', en: 'Drain Pump', ja: '排水ポンプ' },
    'tpa.refillPump': { pt: 'Bomba de Recalque (Enchimento)', en: 'Refill Pump', ja: '給水ポンプ' },
    'tpa.autoCalibrated': { pt: '(Vazão atualizada automaticamente a cada ciclo de TPA)', en: '(Flow updated automatically every TPA cycle)', ja: '（TPAサイクルごとに流量が自動更新されます）' },
    'tpa.pumpProgress': { pt: 'Progresso (L):', en: 'Progress (L):', ja: '進行状況 (L):' },
    'tpa.pumpTime': { pt: 'Tempo:', en: 'Time:', ja: '時間:' },
    'tpa.manual': { pt: 'Controles Manuais', en: 'Manual Controls', ja: '手動制御' },
    'tpa.summary': { pt: '{p}% a cada {n} dia(s), às {t}', en: '{p}% every {n} day(s), at {t}', ja: '{n}日ごと {t} に {p}%' },
    'tpa.canisterOn': { pt: 'Ligar Canister', en: 'Turn Canister On', ja: 'キャニスターON' },
    'tpa.canisterOff': { pt: 'Desligar Canister', en: 'Turn Canister Off', ja: 'キャニスターOFF' },

    // ---- FertsTab ----
    'fert.loading': { pt: 'Carregando fertilizantes...', en: 'Loading fertilizers...', ja: '肥料を読み込み中...' },
    'fert.selectChannel': { pt: 'Selecionar Canal', en: 'Select Channel', ja: 'チャンネル選択' },
    'fert.channel': { pt: 'CANAL', en: 'CHANNEL', ja: 'チャンネル' },
    'fert.refill': { pt: 'Reabastecimento', en: 'Refill Stock', ja: '補充' },
    'fert.newVolume': { pt: 'Volume Novo (mL)', en: 'New Volume (mL)', ja: '新しい量（mL）' },
    'fert.namePlaceholder': { pt: 'Nome (Ex: Ferro)', en: 'Name (e.g. Iron)', ja: '名前（例: 鉄分）' },
    'fert.schedule': { pt: 'AGENDA', en: 'SCHEDULE', ja: 'スケジュール' },
    'fert.totalWeek': { pt: 'TOTAL/SEM', en: 'TOTAL/WK', ja: '週合計' },
    'fert.runOut': { pt: 'Acaba em {d} · {n} dias', en: 'Runs out {d} · {n} days', ja: '残り{n}日 · {d}に終了' },
    'fert.bottleSize': { pt: 'Volume do frasco (mL)', en: 'Bottle size (mL)', ja: 'ボトル容量（mL）' },
    'fert.bottleSizeHint': { pt: 'Quanto o frasco cheio comporta. A barra de estoque e a data de término são calculadas sobre este valor.', en: 'How much a full bottle holds. The stock bar and the run-out date are both measured against it.', ja: '満量時のボトル容量。在庫バーと終了予定日はこの値を基準に計算されます。' },
    'fert.hour': { pt: 'Hora', en: 'Hour', ja: '時' },
    'fert.min': { pt: 'Min', en: 'Min', ja: '分' },
    'fert.save': { pt: 'Salvar', en: 'Save', ja: '保存' },
    'fert.calibPower': { pt: 'CALIBRAÇÃO / POTÊNCIA', en: 'CALIBRATION / POWER', ja: 'キャリブ / 出力' },
    'fert.power': { pt: 'POTÊNCIA (PWM)', en: 'POWER (PWM)', ja: '出力(PWM)' },
    'fert.holdPurge': { pt: 'Segure = Purgar', en: 'Hold = Purge', ja: '長押し = パージ' },
    'fert.run3s': { pt: 'Rodar 3s', en: 'Run 3s', ja: '3秒実行' },
    'fert.mlMeasured': { pt: 'mL medidos', en: 'mL measured', ja: '計測mL' },
    'fert.measureResult': { pt: 'Resultado da Medição (mL)', en: 'Measurement Result (mL)', ja: '計測結果（mL）' },
    'fert.calculate': { pt: 'Calcular', en: 'Calculate', ja: '計算' },
    'tpa.goalEquals': { pt: '= {liters} L do aquário', en: '= {liters} L of the aquarium', ja: '= 水槽の{liters} L' },
    'tpa.recalibrateBoth': { pt: '⚙ Recalibrar as duas bombas', en: '⚙ Recalibrate both pumps', ja: '⚙ 両ポンプを再キャリブレーション' },
    'tpa.calibrateBoth': { pt: '⚙ Calibrar as duas bombas', en: '⚙ Calibrate both pumps', ja: '⚙ 両ポンプをキャリブレーション' },
    'tpa.calibrateBothHint': { pt: 'Drena um pouco e devolve a mesma quantidade, medindo as duas vazões. O nível volta ao ponto de partida. Precisa de água no reservatório.', en: 'Drains a little and puts the same amount back, measuring both flow rates. The level ends where it started. Needs water in the reservoir.', ja: '少量を排水し同量を戻して、両方の流量を測定します。水位は元に戻ります。リザーバーに水が必要です。' },
    'confirm.calibrateBoth': { pt: 'Calibrar as duas bombas? O aquário vai baixar cerca de {pct}% e voltar em seguida. O canister fica desligado durante a medição. Confirme que há água no reservatório.', en: 'Calibrate both pumps? The aquarium drops about {pct}% and comes straight back. The canister stays off while measuring. Make sure the reservoir has water.', ja: '両ポンプをキャリブレーションしますか？水位が約{pct}%下がってすぐ戻ります。測定中はキャニスターが停止します。リザーバーに水があることを確認してください。' },
    'tpa.goalFree': { pt: 'Sem meta — a bomba roda até você apertar OFF.', en: 'No goal — the pump runs until you press OFF.', ja: '目標なし — OFFを押すまでポンプが動作します。' },
    'tpa.goalNotTrackable': { pt: 'Sem sensor de nível nem vazão calibrada — calibre antes de usar meta.', en: 'No level sensor and no calibrated flow — calibrate before using a goal.', ja: '水位センサーもキャリブレーション済み流量もありません。目標を使う前にキャリブレーションしてください。' },
    'tpa.goalOverMax': { pt: 'Máximo {max}% — limitado pelo volume do reservatório.', en: 'Maximum {max}% — capped by the reservoir volume.', ja: '最大{max}% — リザーバー容量による上限です。' },
    'net.error': { pt: 'Falha na conexão com o controlador. O comando NÃO foi enviado.', en: 'Connection to the controller failed. The command was NOT sent.', ja: 'コントローラーへの接続に失敗しました。コマンドは送信されていません。' },
    'net.stale': { pt: '⚠️ Sem conexão — os dados abaixo podem estar desatualizados.', en: '⚠️ Disconnected — the data below may be out of date.', ja: '⚠️ 接続なし — 以下のデータは古い可能性があります。' },
    'confirm.tpaStart': { pt: 'Iniciar a TPA agora? O filtro canister será desligado, {pct}% da água será drenada e as bombas vão operar por vários minutos.', en: 'Start the water change now? The canister filter will switch off, {pct}% of the water will be drained and the pumps will run for several minutes.', ja: '今すぐ換水を開始しますか？キャニスターフィルターが停止し、水の{pct}%が排水され、ポンプが数分間動作します。' },
    'confirm.tpaAbort': { pt: 'Abortar a TPA em andamento? Todas as bombas param imediatamente e o ciclo não será concluído.', en: 'Abort the running water change? All pumps stop immediately and the cycle will not complete.', ja: '実行中の換水を中止しますか？全ポンプが即座に停止し、サイクルは完了しません。' },
    'confirm.pumpDrain': { pt: 'Ligar a bomba de DRENAGEM manualmente? Ela vai retirar água do aquário até você parar ou atingir a meta.', en: 'Run the DRAIN pump manually? It will remove water from the aquarium until you stop it or the goal is reached.', ja: '排水ポンプを手動で運転しますか？停止するか目標に達するまで水槽から水を抜きます。' },
    'confirm.tpaResetLastRun': { pt: 'Zerar a data da última TPA? A próxima vai acontecer no próximo horário agendado.', en: 'Clear the last water change date? The next one will run at the next scheduled time.', ja: '最後の水換え日をクリアしますか？次回は次の予定時刻に実行されます。' },
    'confirm.tpaMarkLastRun': { pt: 'Registrar uma TPA feita agora? A próxima só acontece depois do intervalo completo.', en: 'Record a water change as done now? The next one waits a full interval.', ja: '今、水換えを実施したと記録しますか？次回は間隔が完全に経過してからです。' },
    'confirm.pumpRefill': { pt: 'Ligar a bomba de RECALQUE manualmente? Ela vai enviar água do reservatório para o aquário até você parar ou atingir a meta.', en: 'Run the REFILL pump manually? It will pump water from the reservoir into the aquarium until you stop it or the goal is reached.', ja: '給水ポンプを手動で運転しますか？停止するか目標に達するまでリザーバーから水槽へ送水します。' },
    'confirm.solenoid': { pt: 'Abrir a válvula solenoide? Água da torneira vai encher o reservatório até você fechar.', en: 'Open the solenoid valve? Tap water will fill the reservoir until you close it.', ja: 'ソレノイドバルブを開きますか？閉じるまで水道水がリザーバーに供給されます。' },
    'confirm.canisterOff': { pt: 'Desligar o filtro canister? O aquário fica sem filtragem biológica enquanto estiver desligado.', en: 'Switch off the canister filter? The aquarium has no biological filtration while it is off.', ja: 'キャニスターフィルターを停止しますか？停止中は生物ろ過が働きません。' },
    'confirm.emergency': { pt: 'PARADA DE EMERGÊNCIA: desligar TODOS os atuadores imediatamente — bombas, solenoide e filtro canister. Confirmar?', en: 'EMERGENCY STOP: switch off ALL actuators immediately — pumps, solenoid and canister filter. Confirm?', ja: '緊急停止：全てのアクチュエーター（ポンプ、ソレノイド、キャニスターフィルター）を即座に停止します。実行しますか？' },
    'config.emergencyClear': { pt: '✅ Sair da emergência', en: '✅ Clear emergency', ja: '✅ 緊急状態を解除' },
    'config.emergencyClearHint': { pt: 'Libera o sistema depois de você verificar que está tudo bem. Enquanto a emergência estiver ativa, nada mais funciona.', en: 'Releases the system once you have checked that all is well. While the emergency is active, nothing else runs.', ja: '問題がないことを確認してからシステムを解除します。緊急状態の間は他の機能がすべて停止します。' },
    'confirm.emergencyClear': { pt: 'Sair do estado de emergência? Confirme antes que a causa foi resolvida — nível de água, sensor no lugar e mangueiras em ordem.', en: 'Leave the emergency state? Confirm first that the cause is resolved — water level, sensor in place and hoses in order.', ja: '緊急状態を解除しますか？水位、センサーの位置、ホースの状態など、原因が解消されたことを先に確認してください。' },
    'config.emergencyStop': { pt: '🛑 Parada de emergência', en: '🛑 Emergency stop', ja: '🛑 緊急停止' },
    'config.emergencyHint': { pt: 'Desliga todos os atuadores na hora. Use se algo estiver claramente errado.', en: 'Switches off every actuator at once. Use it if something is clearly wrong.', ja: '全アクチュエーターを即座に停止します。明らかに異常がある場合に使用してください。' },
    'fert.confirmRun3s': { pt: 'Ativar canal {ch} por exatamente 3 segundos? Tenha um recipiente pronto para medir a saída em mL.', en: 'Activate channel {ch} for exactly 3 seconds? Have a container ready to measure output in mL.', ja: 'チャンネル{ch}を3秒間作動させますか？mLを計測する容器を用意してください。' },
    'fert.confirmCalib': { pt: 'Usar {ml}mL como medida de calibração de 3 segundos no canal {ch}?', en: 'Use {ml}mL as 3-second calibration for channel {ch}?', ja: '{ml}mLをチャンネル{ch}の3秒キャリブレーション値として使用しますか？' },
    'fert.enterMl': { pt: 'Insira a quantidade de mL medida.', en: 'Enter the measured mL amount.', ja: '計測したmL量を入力してください。' },
    // Three letters, not one. 'D,S,T,Q,Q,S,S' has three S and two Q: nothing on
    // the row says which day it is, and a dose typed one row off is silent —
    // the controller reads 0 mL for today and never runs the pump.
    'fert.shortDays': { pt: 'Dom,Seg,Ter,Qua,Qui,Sex,Sáb', en: 'Sun,Mon,Tue,Wed,Thu,Fri,Sat', ja: '日,月,火,水,木,金,土' },
    'fert.today': { pt: 'hoje', en: 'today', ja: '今日' },
    'fert.configure': { pt: 'Configurar', en: 'Configure', ja: '設定' },
    'fert.dangerZone': { pt: 'ZONA DE RISCO', en: 'DANGER ZONE', ja: '危険操作' },
    'fert.reset': { pt: 'Resetar este canal', en: 'Reset this channel', ja: 'このチャンネルをリセット' },
    'fert.resetHint': { pt: 'Volta doses, horários, estoque, calibração, PWM e nome ao padrão de fábrica. Use quando a configuração salva estiver errada e o agendamento não disparar.', en: 'Puts doses, times, stock, calibration, PWM and name back to factory defaults. Use it when the stored configuration is wrong and the schedule will not fire.', ja: '投与量・時刻・在庫・キャリブレーション・PWM・名前を工場出荷時に戻します。保存された設定が壊れてスケジュールが動かない場合に使用します。' },
    'fert.confirmReset': { pt: 'Apagar TODA a configuração do canal {ch}? Doses, horários, estoque, calibração, PWM e nome voltam ao padrão. Não dá para desfazer.', en: 'Erase ALL configuration for channel {ch}? Doses, times, stock, calibration, PWM and name go back to defaults. This cannot be undone.', ja: 'チャンネル{ch}の設定をすべて消去しますか？投与量・時刻・在庫・キャリブレーション・PWM・名前が既定値に戻ります。元に戻せません。' },
    'fert.resetDone': { pt: 'Canal {ch} resetado. Configure a agenda novamente.', en: 'Channel {ch} reset. Set the schedule up again.', ja: 'チャンネル{ch}をリセットしました。スケジュールを再設定してください。' },

    // ---- ConfigTab ----
    'config.aquarium': { pt: 'Configuração do Aquário', en: 'Aquarium Configuration', ja: '水槽設定' },
    'config.dimensions': { pt: 'Dimensões do Aquário (cm)', en: 'Aquarium Dimensions (cm)', ja: '水槽サイズ（cm）' },
    'config.height': { pt: 'Altura', en: 'Height', ja: '高さ' },
    'config.heightLabel': { pt: 'Altura (A)', en: 'Height (H)', ja: '高さ(H)' },
    'config.length': { pt: 'Compr.', en: 'Length', ja: '長さ' },
    'config.lengthLabel': { pt: 'Comprimento (C)', en: 'Length (L)', ja: '長さ(L)' },
    'config.width': { pt: 'Largura', en: 'Width', ja: '幅' },
    'config.widthLabel': { pt: 'Largura (L)', en: 'Width (W)', ja: '幅(W)' },
    'config.margin': { pt: 'Margem da borda (mm)', en: 'Edge margin (mm)', ja: '上端マージン（mm）' },
    'config.marginHint': { pt: 'Distância da borda do vidro até onde começa o limite de transbordo da água.', en: 'Distance from the glass rim down to the water overflow limit.', ja: 'ガラスの縁から溢水限界までの距離。' },
    'config.sensorSection': { pt: 'Sensor de Nível', en: 'Level Sensor', ja: '水位センサー' },
    'config.reservoirSection': { pt: 'Reservatório e Prime', en: 'Reservoir & Prime', ja: 'リザーバーとプライム' },
    'config.sensorFull': { pt: 'Distância 100% (Sensor até água)', en: '100% Distance (Sensor to water)', ja: '100%の距離（センサーから水面）' },
    'config.sensorFullHint': { pt: 'Distância do sensor até a água quando o aquário está cheio', en: 'Distance from sensor to water when tank is full', ja: '満水時のセンサーから水面までの距離' },
    'config.calibrateSensor': { pt: 'Calibrar 100%', en: 'Calibrate 100%', ja: '100%をキャリブレーション' },
    'config.calcVolume': { pt: 'Volume Calculado:', en: 'Calculated Volume:', ja: '計算された容量:' },
    'config.litersPerCm': { pt: 'Litros por cm:', en: 'Liters per cm:', ja: 'cm当たりリットル:' },
    'config.primeRatio': { pt: 'Proporção de Prime (mL por Litro)', en: 'Prime Ratio (mL per Liter)', ja: 'プライム比率（mL/L）' },
    'config.primeHint': { pt: 'Conforme recomendação do fabricante (Ex: Seachem Prime = 0.05 mL/L)', en: 'Per manufacturer recommendation (e.g. Seachem Prime = 0.05 mL/L)', ja: 'メーカー推奨値（例: Seachem Prime = 0.05 mL/L）' },
    'config.feedPause': { pt: 'Pausa para alimentação (min)', en: 'Feeding pause (min)', ja: '給餌の停止時間（分）' },
    'config.feedPauseHint': { pt: 'Quanto tempo o canister fica desligado ao tocar em Alimentar. O firmware religa sozinho ao fim do tempo.', en: 'How long the canister stays off after tapping Feed. The firmware switches it back on when the time is up.', ja: '「給餌」をタップした後キャニスターを停止する時間。時間経過後はファームウェアが自動で再開します。' },
    'config.canisterSafe': { pt: 'Nível seguro do canister (%)', en: 'Canister safe level (%)', ja: 'キャニスター安全水位 (%)' },
    'config.canisterSafeHint': { pt: 'Altura mínima da água, em % do aquário, para religar o canister após um erro. Abaixo disso ele fica desligado para não funcionar a seco.', en: 'Minimum water height, as % of the aquarium, to switch the canister back on after an error. Below it the filter stays off so it does not run dry.', ja: 'エラー後にキャニスターを再始動するための最低水位（水槽に対する%）。これを下回る場合、空運転を避けるため停止したままにします。' },
    'config.reservoirVol': { pt: 'Volume do Reservatório de Tratamento (Litros)', en: 'Treatment Reservoir Volume (Liters)', ja: '処理リザーバー容量（リットル）' },
    'config.reservoirHint': { pt: 'Água tratada com Prime antes de repor no aquário', en: 'Water treated with Prime before refilling aquarium', ja: 'プライム処理した水を水槽に戻す前の量' },
    'config.calcPrime': { pt: 'Dose de Prime calculada (reservatório):', en: 'Calculated Prime dose (reservoir):', ja: 'プライム投与量（計算値）:' },
    'config.calcPrimeHint': { pt: 'Configure volume do reservatório e proporção', en: 'Set reservoir volume and ratio', ja: 'リザーバー容量と比率を設定' },
    'config.saveConfig': { pt: 'Salvar Configuração', en: 'Save Configuration', ja: '設定保存' },
    'config.network': { pt: 'Configuração de Rede', en: 'Network Configuration', ja: 'ネットワーク設定' },
    'config.scanning': { pt: 'Buscando...', en: 'Scanning...', ja: 'スキャン中...' },
    'config.selectNetwork': { pt: 'Selecione a rede', en: 'Select network', ja: 'ネットワーク選択' },
    'config.noNetworks': { pt: 'Nenhuma rede (clique buscar)', en: 'No networks (click scan)', ja: 'ネットワークなし（スキャンをクリック）' },
    'config.password': { pt: 'Senha da Rede', en: 'Network Password', ja: 'パスワード' },
    'config.saveRestart': { pt: 'Salvar e Reiniciar', en: 'Save & Restart', ja: '保存して再起動' },
    'config.emergency': { pt: 'Ações de Emergência', en: 'Emergency Actions', ja: '緊急操作' },
    'config.pauseTpa': { pt: 'Pausar TPA / Ligar Manutenção', en: 'Pause TPA / Enable Maintenance', ja: 'TPA一時停止 / メンテナンス有効' },
    'config.resumeTpa': { pt: 'Retomar TPA / Sair da Manutenção', en: 'Resume TPA / Exit Maintenance', ja: 'TPA再開 / メンテナンス解除' },
    'config.wifiOk': { pt: 'WiFi configurado! O sistema reiniciará para conectar.', en: 'WiFi configured! System will restart to connect.', ja: 'WiFi設定完了！接続のため再起動します。' },
    'config.wifiError': { pt: 'Erro ao salvar WiFi.', en: 'Error saving WiFi.', ja: 'WiFi保存エラー。' },
    'config.commError': { pt: 'Erro de comunicação.', en: 'Communication error.', ja: '通信エラー。' },
    'config.scanError': { pt: 'Erro ao buscar redes.', en: 'Error scanning networks.', ja: 'ネットワークスキャンエラー。' },
    'config.language': { pt: 'Idioma / Language / 言語', en: 'Language / Idioma / 言語', ja: '言語 / Language / Idioma' },

    // ---- Notifications ----
    'notify.title': { pt: 'Notificações (ntfy.sh)', en: 'Notifications (ntfy.sh)', ja: '通知（ntfy.sh）' },
    'notify.key': { pt: 'Tópico ntfy.sh', en: 'ntfy.sh Topic', ja: 'ntfy.shトピック' },
    'notify.keyHint': { pt: 'Baixe o app ntfy e inscreva-se neste tópico', en: 'Download ntfy app and subscribe to this topic', ja: 'ntfyアプリをダウンロードし、このトピックを購読してください' },
    'notify.save': { pt: 'Salvar Tópico', en: 'Save Topic', ja: 'トピック保存' },
    'notify.test': { pt: '🔔 Enviar Teste', en: '🔔 Send Test', ja: '🔔 テスト送信' },
    'notify.testSent': { pt: 'Notificação de teste enviada!', en: 'Test notification sent!', ja: 'テスト通知を送信しました！' },
    'notify.enabled': { pt: 'Ativo', en: 'Active', ja: '有効' },
    'notify.disabled': { pt: 'Desativado', en: 'Disabled', ja: '無効' },
    'notify.reportTime': { pt: 'Relatório Diário', en: 'Daily Report', ja: '日次レポート' },
    'notify.saveConfig': { pt: 'Salvar Configuração', en: 'Save Config', ja: '設定保存' },
    'notify.tpaComplete': { pt: 'TPA Concluída', en: 'TPA Complete', ja: 'TPA完了' },
    'notify.tpaError': { pt: 'Erro na TPA', en: 'TPA Error', ja: 'TPAエラー' },
    'notify.fertLowStock': { pt: 'Estoque Baixo', en: 'Low Stock', ja: '在庫低下' },
    'notify.emergency': { pt: 'Emergência', en: 'Emergency', ja: '緊急' },
    'notify.fertComplete': { pt: 'Fertilização OK', en: 'Fertilization OK', ja: '施肥完了' },
    'config.save': { pt: 'SALVAR CONFIGURAÇÕES', en: 'SAVE CONFIG', ja: '設定を保存' },
    'config.success': { pt: 'Salvo com sucesso!', en: 'Saved successfully!', ja: '保存しました！' },
    'config.otaTitle': { pt: 'Atualização de Firmware', en: 'Firmware Update', ja: 'ファームウェアの更新' },
    'config.otaSelect': { pt: 'Escolher Arquivo (.bin)', en: 'Choose File (.bin)', ja: 'ファイルを選択 (.bin)' },
    'config.otaUpload': { pt: 'Enviar', en: 'Upload', ja: 'アップロード' },
    'config.otaUploading': { pt: 'Enviando...', en: 'Uploading...', ja: 'アップロード中...' },
    'config.otaSuccess': { pt: 'Sucesso! O sistema irá reiniciar.', en: 'Success! System will restart.', ja: '成功！システムが再起動します。' },
    'config.otaError': { pt: 'Erro no envio.', en: 'Upload Error.', ja: 'アップロードエラー。' },
    
    // Checklist de configurações faltantes
    'config.missing.aqLength': { pt: 'Aquário: Comprimento', en: 'Aquarium: Length', ja: '水槽: 長さ' },
    'config.missing.aqWidth': { pt: 'Aquário: Largura', en: 'Aquarium: Width', ja: '水槽: 幅' },
    'config.missing.aqHeight': { pt: 'Aquário: Altura', en: 'Aquarium: Height', ja: '水槽: 高さ' },
    'config.missing.sensorFullDistanceMm': { pt: 'Aquário: Dist. Cheio (mm)', en: 'Aquarium: Full Dist. (mm)', ja: '水槽: 満水距離 (mm)' },
    'config.missing.reservoirVolume': { pt: 'Reservatório: Volume (L)', en: 'Reservoir: Volume (L)', ja: 'リザーバー: 容量（L）' },
    'config.missing.drainFlowRate': { pt: 'TPA: Vazão Drenagem (Calibrar)', en: 'TPA: Drain Flow (Calibrate)', ja: 'TPA: 排水流量（キャリブレーション）' },
    'config.missing.refillFlowRate': { pt: 'TPA: Vazão Recalque (Calibrar)', en: 'TPA: Refill Flow (Calibrate)', ja: 'TPA: 給水流量（キャリブレーション）' },
    // The TPA state machine's own names. Rendered raw before this, so every
    // user saw FILLING_RESERVOIR and MANUAL_PUMP_DRAIN regardless of language.
    'tpaState.IDLE': { pt: 'Parado', en: 'Idle', ja: '待機中' },
    'tpaState.CANISTER_OFF': { pt: 'Desligando filtro', en: 'Stopping filter', ja: 'フィルター停止中' },
    'tpaState.DRAINING': { pt: 'Drenando', en: 'Draining', ja: '排水中' },
    'tpaState.FILLING_RESERVOIR': { pt: 'Enchendo reservatório', en: 'Filling reservoir', ja: 'リザーバー給水中' },
    'tpaState.DOSING_PRIME': { pt: 'Dosando Prime', en: 'Dosing Prime', ja: 'プライム投与中' },
    'tpaState.REFILLING': { pt: 'Repondo água', en: 'Refilling', ja: '給水中' },
    'tpaState.CANISTER_ON': { pt: 'Religando filtro', en: 'Restarting filter', ja: 'フィルター再起動中' },
    'tpaState.COMPLETE': { pt: 'Concluída', en: 'Complete', ja: '完了' },
    'tpaState.ERROR': { pt: 'Erro', en: 'Error', ja: 'エラー' },
    'tpaState.MANUAL_RESERVOIR_FILL': { pt: 'Enchendo reservatório (manual)', en: 'Filling reservoir (manual)', ja: 'リザーバー給水中（手動）' },
    'tpaState.MANUAL_PUMP_DRAIN': { pt: 'Drenagem manual', en: 'Manual drain', ja: '手動排水' },
    'tpaState.MANUAL_PUMP_REFILL': { pt: 'Recalque manual', en: 'Manual refill', ja: '手動給水' },
    'home.tpaCapped': { pt: 'Limitado a {v} L pelo reservatório', en: 'Capped to {v} L by the reservoir', ja: 'リザーバーにより{v} Lに制限' },
    'home.tpaTopUp': { pt: 'Complete o aquário — falta mais água do que esta troca reporia', en: 'Top the tank up — it is short by more than this change would replace', ja: '水槽に給水してください — 今回の換水で補える量を超えて不足しています' },
    'home.tpaBlocked': { pt: 'Última tentativa recusada: {r}', en: 'Last attempt refused: {r}', ja: '前回の実行は拒否されました: {r}' },
    'home.tpaError': { pt: 'Última TPA falhou: {e}', en: 'Last cycle failed: {e}', ja: '前回のサイクルが失敗: {e}' },
    'home.sensorDown': { pt: 'Sensor sem resposta — o nível mostrado pode estar velho', en: 'Sensor not responding — the level shown may be stale', ja: 'センサー応答なし — 表示中の水位は古い可能性があります' },
    'common.cancel': { pt: 'Cancelar', en: 'Cancel', ja: 'キャンセル' },
    'common.confirm': { pt: 'Confirmar', en: 'Confirm', ja: '実行' },
    'config.missing.canisterSafePct': { pt: 'Canister: Nível Seguro (%)', en: 'Canister: Safe Level (%)', ja: 'キャニスター: 安全水位（%）' },
    'config.missing.reservoirSafetyML': { pt: 'TPA: Margem de Segurança do Reservatório (mL)', en: 'TPA: Reservoir Safety Margin (mL)', ja: 'TPA: リザーバー安全マージン（mL）' },

    'notify.dailyLevel': { pt: 'Nível Diário', en: 'Daily Level', ja: '日次水位' },
    'notify.keySaved': { pt: 'Tópico salvo com sucesso!', en: 'Topic saved successfully!', ja: 'トピックが保存されました！' },
    'notify.noKey': { pt: 'Insira um tópico válido.', en: 'Enter a valid topic.', ja: '有効なトピックを入力してください。' },

    // ---- LogsTab ----
    'nav.logs': { pt: 'Logs', en: 'Logs', ja: 'ログ' },
    'logs.title': { pt: 'Log de Bombas', en: 'Pump Log', ja: 'ポンプログ' },
    'logs.events': { pt: 'eventos', en: 'events', ja: 'イベント' },
    'logs.refresh': { pt: 'Atualizar', en: 'Refresh', ja: '更新' },
    'logs.all': { pt: 'TODOS', en: 'ALL', ja: 'すべて' },
    'logs.empty': { pt: 'Nenhum evento registrado desde o último boot.', en: 'No events recorded since last boot.', ja: '前回起動以降のイベントなし。' },
    'logs.info': { pt: '💡 O log armazena os últimos 100 eventos de bomba em flash (LittleFS). Os dados sobrevivem reboots. Útil para diagnosticar ativações inesperadas durante a noite.', en: '💡 The log stores the last 100 pump events in flash (LittleFS). Data survives reboots. Useful for diagnosing unexpected overnight activations.', ja: '💡 ログはフラッシュ（LittleFS）に最後の100件のポンプイベントを保存します。再起動後もデータは保持されます。夜間の予期せぬ動作の診断に役立ちます。' },
} as const;

type TranslationKey = keyof typeof translations;

/**
 * Narrows a TPA state name coming off the wire into a translation key.
 *
 * The firmware's enum is not part of this file's key union, and a state it
 * gains later would otherwise be a compile error here or an untranslated
 * string in the UI. Returns null for anything unrecognised so the caller can
 * fall back to showing the raw name — which is still better than nothing.
 */
export function tpaStateKey(state: string): TranslationKey | null {
    const key = `tpaState.${state}`;
    return key in translations ? (key as TranslationKey) : null;
}

type I18nContextType = {
    lang: Lang;
    setLang: (lang: Lang) => void;
    t: (key: TranslationKey, params?: Record<string, string | number>) => string;
};

const I18nContext = createContext<I18nContextType>({
    lang: 'pt',
    setLang: () => { },
    t: (key) => key,
});

export function I18nProvider({ children, initialLang }: { children: ReactNode; initialLang?: number }) {
    const langMap: Lang[] = ['pt', 'en', 'ja'];
    const [lang, setLangState] = useState<Lang>(langMap[initialLang ?? 0] || 'pt');

    const setLang = useCallback((newLang: Lang) => {
        setLangState(newLang);
        const langIdx = langMap.indexOf(newLang);
        api('POST', '/api/schedule', { language: langIdx });
    }, []);

    const t = useCallback((key: TranslationKey, params?: Record<string, string | number>): string => {
        const entry = translations[key];
        if (!entry) return key;
        let text = (entry as Record<Lang, string>)[lang] || (entry as Record<Lang, string>).pt || key;
        if (params) {
            Object.entries(params).forEach(([k, v]) => {
                text = text.replaceAll(`{${k}}`, String(v));
            });
        }
        return text;
    }, [lang]);

    return (
        <I18nContext.Provider value={{ lang, setLang, t }
        }>
            {children}
        </I18nContext.Provider>
    );
}

export function useT() {
    return useContext(I18nContext);
}
