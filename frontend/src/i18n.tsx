import { createContext, useContext, useState, useCallback, type ReactNode } from 'react';
import { api } from './App';

export type Lang = 'pt' | 'en' | 'ja';

const translations = {
    // ---- Navigation ----
    'nav.home': { pt: 'Início', en: 'Home', ja: 'ホーム' },
    'nav.tpa': { pt: 'TPA', en: 'TPA', ja: 'TPA' },
    'nav.ferts': { pt: 'Ferts', en: 'Ferts', ja: '肥料' },
    'nav.config': { pt: 'Config', en: 'Config', ja: '設定' },

    // ---- Emergency ----
    'emergency.banner': { pt: '⚠️ EMERGÊNCIA — Sensor detectou risco de transbordamento! Parando bombas imediatamente.', en: '⚠️ EMERGENCY — Sensor detected overflow risk! Stopping pumps immediately.', ja: '⚠️ 緊急事態 — センサーがオーバーフローリスクを検出！ポンプを即時停止。' },

    // ---- HomeTab ----
    'home.sensors': { pt: 'Sensores e Segurança', en: 'Sensors & Safety', ja: 'センサーと安全' },
    'home.waterLevel': { pt: 'Nível de Água', en: 'Water Level', ja: '水位' },
    'home.optical': { pt: 'Nível Ótico', en: 'Optical Level', ja: '光学レベル' },
    'home.opticalOn': { pt: 'CHEIO', en: 'FULL', ja: '満水' },
    'home.opticalOff': { pt: 'NORMAL', en: 'NORMAL', ja: '正常' },
    'home.float': { pt: 'Bóia de Reposição', en: 'Float Switch', ja: 'フロートスイッチ' },
    'home.floatOn': { pt: 'VAZIO', en: 'EMPTY', ja: '空' },
    'home.floatOff': { pt: 'NO NÍVEL', en: 'AT LEVEL', ja: 'レベル内' },
    'home.canister': { pt: 'Bomba Canister', en: 'Canister Pump', ja: 'キャニスターポンプ' },
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
    'home.noSchedule': { pt: 'Nenhum agendamento configurado.', en: 'No schedule configured.', ja: 'スケジュール未設定。' },
    'home.fertTable': { pt: 'Tabela de Fertilizantes', en: 'Fertilizer Table', ja: '肥料テーブル' },
    'home.inMl': { pt: 'Em mL', en: 'In mL', ja: 'mL単位' },
    'home.channel': { pt: 'Canal', en: 'Channel', ja: 'チャンネル' },
    'home.hour': { pt: 'Hora', en: 'Hour', ja: '時間' },
    'home.waiting': { pt: 'Aguardando dados...', en: 'Waiting for data...', ja: 'データ待ち...' },
    'home.noActiveSchedule': { pt: 'Nenhum agendamento ativo na semana.', en: 'No active schedule this week.', ja: '今週のスケジュールなし。' },
    'home.shortDays': { pt: 'D,S,T,Q,Q,S,S', en: 'S,M,T,W,T,F,S', ja: '日,月,火,水,木,金,土' },

    // ---- TPATab ----
    'tpa.auto': { pt: 'TPA Automática', en: 'Auto TPA', ja: '自動TPA' },
    'tpa.frequency': { pt: 'Frequência (Dias)', en: 'Frequency (Days)', ja: '頻度（日数）' },
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
    'tpa.incompleteConfig': { pt: '⚠ Configuração Incompleta', en: '⚠ Incomplete Configuration', ja: '⚠ 設定不完全' },
    'tpa.incompleteMsg': { pt: 'A TPA não será executada até que todos os campos obrigatórios sejam preenchidos na aba', en: 'TPA will not run until all required fields are filled in the', ja: '必須項目がすべて入力されるまでTPAは実行されません。' },
    'tpa.dimMissing': { pt: 'Dimensões do aquário (A×C×L)', en: 'Aquarium dimensions (H×L×W)', ja: '水槽サイズ（高×長×幅）' },
    'tpa.reservoirMissing': { pt: 'Volume do reservatório', en: 'Reservoir volume', ja: 'リザーバー容量' },
    'tpa.pctMissing': { pt: 'Porcentagem da TPA', en: 'TPA percentage', ja: 'TPA交換率' },
    'tpa.reservoir': { pt: 'Reservatório & Bombas', en: 'Reservoir & Pumps', ja: 'リザーバー＆ポンプ' },
    'tpa.safetyMargin': { pt: 'Margem de Segurança do Reservatório (mL)', en: 'Reservoir Safety Margin (mL)', ja: 'リザーバー安全マージン（mL）' },
    'tpa.safetyHint': { pt: 'Volume mínimo que deve ficar no reservatório para a bomba de recalque não secar', en: 'Minimum volume to keep in reservoir so refill pump doesn\'t run dry', ja: 'リフィルポンプが空回りしないための最低量' },
    'tpa.testDrain': { pt: '🔽 Testar Diafragma', en: '🔽 Test Drain', ja: '🔽 排水テスト' },
    'tpa.testRefill': { pt: '🔼 Testar Recalque', en: '🔼 Test Refill', ja: '🔼 給水テスト' },
    'tpa.primeDose': { pt: 'Dose de Prime na Reposição', en: 'Prime Dose for Refill', ja: 'リフィル用プライム投与量' },
    'tpa.configInConfigTab': { pt: 'Configure na aba Config', en: 'Set in Config tab', ja: 'Configタブで設定' },
    'tpa.autoCalc': { pt: '(calculada automaticamente: reservatório × proporção)', en: '(auto-calculated: reservoir × ratio)', ja: '（自動計算: リザーバー × 比率）' },
    'tpa.saveConfig': { pt: 'Salvar Configuração', en: 'Save Config', ja: '設定保存' },
    'tpa.pumpCalib': { pt: 'Calibração das Bombas', en: 'Pump Calibration', ja: 'ポンプキャリブレーション' },
    'tpa.drainPump': { pt: 'Bomba Diafragma (Esvaziamento)', en: 'Drain Pump (Diaphragm)', ja: '排水ポンプ（ダイヤフラム）' },
    'tpa.running': { pt: 'Rodando...', en: 'Running...', ja: '動作中...' },
    'tpa.run3s': { pt: 'Rodar 3s', en: 'Run 3s', ja: '3秒実行' },
    'tpa.mlMeasured': { pt: 'mL medidos', en: 'mL measured', ja: '計測mL' },
    'tpa.calibrate': { pt: 'Calibrar', en: 'Calibrate', ja: 'キャリブ' },
    'tpa.flowRate': { pt: 'Vazão atual:', en: 'Current flow:', ja: '現在の流量:' },
    'tpa.notCalibrated': { pt: 'não calibrada', en: 'not calibrated', ja: '未キャリブ' },
    'tpa.purgeDrain': { pt: '🔁 Segurar para Purgar Diafragma', en: '🔁 Hold to Purge Drain', ja: '🔁 長押しでパージ（排水）' },
    'tpa.refillPump': { pt: 'Bomba de Recalque (Enchimento)', en: 'Refill Pump', ja: '給水ポンプ' },
    'tpa.purgeRefill': { pt: '🔁 Segurar para Purgar Recalque', en: '🔁 Hold to Purge Refill', ja: '🔁 長押しでパージ（給水）' },

    // ---- FertsTab ----
    'fert.loading': { pt: 'Carregando fertilizantes...', en: 'Loading fertilizers...', ja: '肥料を読み込み中...' },
    'fert.selectChannel': { pt: 'Selecionar Canal', en: 'Select Channel', ja: 'チャンネル選択' },
    'fert.channel': { pt: 'CANAL', en: 'CHANNEL', ja: 'チャンネル' },
    'fert.refill': { pt: 'Reabastecimento', en: 'Refill Stock', ja: '補充' },
    'fert.newVolume': { pt: 'Volume Novo (mL)', en: 'New Volume (mL)', ja: '新しい量（mL）' },
    'fert.namePlaceholder': { pt: 'Nome (Ex: Ferro)', en: 'Name (e.g. Iron)', ja: '名前（例: 鉄分）' },
    'fert.schedule': { pt: 'AGENDA', en: 'SCHEDULE', ja: 'スケジュール' },
    'fert.totalWeek': { pt: 'TOTAL/SEM', en: 'TOTAL/WK', ja: '週合計' },
    'fert.hour': { pt: 'Hora', en: 'Hour', ja: '時' },
    'fert.min': { pt: 'Mino', en: 'Min', ja: '分' },
    'fert.save': { pt: 'Salvar', en: 'Save', ja: '保存' },
    'fert.calibPower': { pt: 'CALIBRAÇÃO / POTÊNCIA', en: 'CALIBRATION / POWER', ja: 'キャリブ / 出力' },
    'fert.power': { pt: 'POTÊNCIA (PWM)', en: 'POWER (PWM)', ja: '出力(PWM)' },
    'fert.holdPurge': { pt: 'Segure = Purgar', en: 'Hold = Purge', ja: '長押し = パージ' },
    'fert.run3s': { pt: 'Rodar 3s', en: 'Run 3s', ja: '3秒実行' },
    'fert.measureResult': { pt: 'Resultado da Medição (mL)', en: 'Measurement Result (mL)', ja: '計測結果（mL）' },
    'fert.calculate': { pt: 'Calcular', en: 'Calculate', ja: '計算' },
    'fert.confirmRun3s': { pt: 'Ativar canal {ch} por exatamente 3 segundos? Tenha um recipiente pronto para medir a saída em mL.', en: 'Activate channel {ch} for exactly 3 seconds? Have a container ready to measure output in mL.', ja: 'チャンネル{ch}を3秒間作動させますか？mLを計測する容器を用意してください。' },
    'fert.confirmCalib': { pt: 'Usar {ml}mL como medida de calibração de 3 segundos no canal {ch}?', en: 'Use {ml}mL as 3-second calibration for channel {ch}?', ja: '{ml}mLをチャンネル{ch}の3秒キャリブレーション値として使用しますか？' },
    'fert.enterMl': { pt: 'Insira a quantidade de mL medida.', en: 'Enter the measured mL amount.', ja: '計測したmL量を入力してください。' },
    'fert.shortDays': { pt: 'D,S,T,Q,Q,S,S', en: 'S,M,T,W,T,F,S', ja: '日,月,火,水,木,金,土' },

    // ---- ConfigTab ----
    'config.aquarium': { pt: 'Configuração do Aquário', en: 'Aquarium Configuration', ja: '水槽設定' },
    'config.dimensions': { pt: 'Dimensões do Aquário (cm)', en: 'Aquarium Dimensions (cm)', ja: '水槽サイズ（cm）' },
    'config.height': { pt: 'Altura', en: 'Height', ja: '高さ' },
    'config.heightLabel': { pt: 'Altura (A)', en: 'Height (H)', ja: '高さ(H)' },
    'config.length': { pt: 'Compr.', en: 'Length', ja: '長さ' },
    'config.lengthLabel': { pt: 'Comprimento (C)', en: 'Length (L)', ja: '長さ(L)' },
    'config.width': { pt: 'Largura', en: 'Width', ja: '幅' },
    'config.widthLabel': { pt: 'Largura (L)', en: 'Width (W)', ja: '幅(W)' },
    'config.margin': { pt: 'Margem da borda (cm)', en: 'Edge margin (cm)', ja: '上端マージン（cm）' },
    'config.marginHint': { pt: 'Distância da borda do aquário até a superfície da água quando cheio', en: 'Distance from aquarium edge to water surface when full', ja: '満水時の水槽上端から水面までの距離' },
    'config.calcVolume': { pt: 'Volume calculado:', en: 'Calculated volume:', ja: '計算容量:' },
    'config.litersPerCm': { pt: 'Litros por cm:', en: 'Liters per cm:', ja: 'cm当たりリットル:' },
    'config.primeRatio': { pt: 'Proporção de Prime (mL por Litro)', en: 'Prime Ratio (mL per Liter)', ja: 'プライム比率（mL/L）' },
    'config.primeHint': { pt: 'Conforme recomendação do fabricante (Ex: Seachem Prime = 0.05 mL/L)', en: 'Per manufacturer recommendation (e.g. Seachem Prime = 0.05 mL/L)', ja: 'メーカー推奨値（例: Seachem Prime = 0.05 mL/L）' },
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
    'config.wifiOk': { pt: 'WiFi configurado! O sistema reiniciará para conectar.', en: 'WiFi configured! System will restart to connect.', ja: 'WiFi設定完了！接続のため再起動します。' },
    'config.wifiError': { pt: 'Erro ao salvar WiFi.', en: 'Error saving WiFi.', ja: 'WiFi保存エラー。' },
    'config.commError': { pt: 'Erro de comunicação.', en: 'Communication error.', ja: '通信エラー。' },
    'config.scanError': { pt: 'Erro ao buscar redes.', en: 'Error scanning networks.', ja: 'ネットワークスキャンエラー。' },
    'config.language': { pt: 'Idioma / Language / 言語', en: 'Language / Idioma / 言語', ja: '言語 / Language / Idioma' },

    // ---- Notifications ----
    'notify.title': { pt: 'Notificações (Pushsafer)', en: 'Notifications (Pushsafer)', ja: '通知（Pushsafer）' },
    'notify.key': { pt: 'Chave Privada', en: 'Private Key', ja: 'プライベートキー' },
    'notify.keyHint': { pt: 'Obtenha em pushsafer.com → Dashboard → API', en: 'Get from pushsafer.com → Dashboard → API', ja: 'pushsafer.com → Dashboard → APIから取得' },
    'notify.save': { pt: 'Salvar Chave', en: 'Save Key', ja: 'キー保存' },
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
    'notify.dailyLevel': { pt: 'Nível Diário', en: 'Daily Level', ja: '日次水位' },
    'notify.keySaved': { pt: 'Chave salva com sucesso!', en: 'Key saved successfully!', ja: 'キーが保存されました！' },
    'notify.noKey': { pt: 'Insira uma chave válida.', en: 'Enter a valid key.', ja: '有効なキーを入力してください。' },
} as const;

type TranslationKey = keyof typeof translations;

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
