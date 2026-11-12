# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""The translations of the interface of Dragon UnPACKer 6, made by hand for apply_translations.py.

TRANSLATIONS[source] = (es, it, fr, de, pl, ru, zh_CN, ja, ar, he)
The key is the source text without the "&" of the keyboard mnemonics (the translations have none, but the ones of
pt_BR, which are kept in the .ts file); PT_BR[source] are the Brazilian Portuguese texts of the strings added after
the first translation.
"""

LANGUAGES = ["es", "it", "fr", "de", "pl", "ru", "zh_CN", "ja", "ar", "he"]

TRANSLATIONS = {
    "Too much to drag out (%1 files): use Extract": (
        "Demasiados archivos para arrastrar (%1): use Extraer",
        "Troppi file da trascinare (%1): usare Estrai",
        "Trop de fichiers à glisser (%1) : utilisez Extraire",
        "Zu viele Dateien zum Herausziehen (%1): bitte Extrahieren verwenden",
        "Zbyt wiele plików do przeciągnięcia (%1): użyj Wypakuj",
        "Слишком много файлов для перетаскивания (%1): используйте «Извлечь»",
        "要拖出的文件太多（%1 个）：请使用“提取”",
        "ドラッグするファイルが多すぎます（%1 件）：「展開」を使用してください",
        "عدد الملفات أكبر من أن يُسحب (%1): استخدم «استخراج»",
        "יותר מדי קבצים לגרירה (%1): השתמשו ב״חלץ״"),
    "Name": ("Nombre", "Nome", "Nom", "Name", "Nazwa", "Имя", "名称", "名前", "الاسم", "שם"),
    "Type": ("Tipo", "Tipo", "Type", "Typ", "Typ", "Тип", "类型", "種類", "النوع", "סוג"),
    "Size": ("Tamaño", "Dimensione", "Taille", "Größe", "Rozmiar", "Размер", "大小", "サイズ", "الحجم", "גודל"),
    "Offset": ("Desplazamiento", "Offset", "Décalage", "Offset", "Przesunięcie", "Смещение", "偏移", "オフセット", "الإزاحة", "היסט"),
    "Folder": ("Carpeta", "Cartella", "Dossier", "Ordner", "Folder", "Папка", "文件夹", "フォルダー", "المجلد", "תיקייה"),
    "Scan": ("Analizar", "Scansiona", "Analyser", "Durchsuchen", "Skanuj", "Сканировать", "扫描", "スキャン", "فحص", "סרוק"),
    "Open the files found": (
        "Abrir los archivos encontrados", "Apri i file trovati", "Ouvrir les fichiers trouvés", "Gefundene Dateien öffnen",
        "Otwórz znalezione pliki", "Открыть найденные файлы", "打开找到的文件", "見つかったファイルを開く", "فتح الملفات التي تم العثور عليها", "פתח את הקבצים שנמצאו"),
    "HyperRipper": ("HyperRipper",) * 10,
    "Looks for files embedded in any file by their signatures (images, sounds, archives...).": (
        "Busca archivos incrustados en cualquier archivo por sus firmas (imágenes, sonidos, archivos comprimidos...).",
        "Cerca file incorporati in qualsiasi file tramite le loro firme (immagini, suoni, archivi...).",
        "Recherche les fichiers intégrés dans n'importe quel fichier grâce à leurs signatures (images, sons, archives...).",
        "Sucht in beliebigen Dateien anhand ihrer Signaturen nach eingebetteten Dateien (Bilder, Töne, Archive ...).",
        "Szuka plików osadzonych w dowolnym pliku na podstawie ich sygnatur (obrazy, dźwięki, archiwa...).",
        "Ищет файлы, встроенные в любой файл, по их сигнатурам (изображения, звуки, архивы...).",
        "根据特征签名在任意文件中查找嵌入的文件（图像、声音、归档等）。",
        "シグネチャを使って、任意のファイルに埋め込まれたファイル（画像、サウンド、アーカイブなど）を探します。",
        "يبحث عن الملفات المضمَّنة في أي ملف عبر توقيعاتها (صور، أصوات، أرشيفات...).",
        "מחפש קבצים שמוטמעים בכל קובץ לפי החתימות שלהם (תמונות, צלילים, ארכיונים...)."),
    "Looks for files embedded in any file by their signatures (images, sounds, videos, archives...).": (
        "Busca archivos incrustados en cualquier archivo por sus firmas (imágenes, sonidos, vídeos, archivos comprimidos...).",
        "Cerca file incorporati in qualsiasi file tramite le loro firme (immagini, suoni, video, archivi...).",
        "Recherche les fichiers intégrés dans n'importe quel fichier grâce à leurs signatures (images, sons, vidéos, archives...).",
        "Sucht in beliebigen Dateien anhand ihrer Signaturen nach eingebetteten Dateien (Bilder, Töne, Videos, Archive ...).",
        "Szuka plików osadzonych w dowolnym pliku na podstawie ich sygnatur (obrazy, dźwięki, filmy, archiwa...).",
        "Ищет файлы, встроенные в любой файл, по их сигнатурам (изображения, звуки, видео, архивы...).",
        "根据特征签名在任意文件中查找嵌入的文件（图像、声音、视频、归档等）。",
        "シグネチャを使って、任意のファイルに埋め込まれたファイル（画像、サウンド、動画、アーカイブなど）を探します。",
        "يبحث عن الملفات المضمَّنة في أي ملف عبر توقيعاتها (صور، أصوات، مقاطع فيديو، أرشيفات...).",
        "מחפש קבצים שמוטמעים בכל קובץ לפי החתימות שלהם (תמונות, צלילים, סרטונים, ארכיונים...)."),
    "Leave out the formats that give many false positives": (
        "Omitir los formatos que dan muchos falsos positivos", "Escludi i formati che danno molti falsi positivi",
        "Exclure les formats qui donnent beaucoup de faux positifs", "Formate mit vielen Fehltreffern auslassen",
        "Pomiń formaty dające wiele fałszywych trafień", "Исключить форматы с большим числом ложных срабатываний",
        "排除误报较多的格式", "誤検出の多い形式を除外する", "استبعاد الصيغ التي تعطي نتائج إيجابية خاطئة كثيرة", "השמט פורמטים שנותנים הרבה תוצאות שגויות"),
    "MP3 / AAC options...": (
        "Opciones de MP3 / AAC...", "Opzioni MP3 / AAC...", "Options MP3 / AAC...", "MP3-/AAC-Optionen ...", "Opcje MP3 / AAC...",
        "Параметры MP3 / AAC...", "MP3 / AAC 选项...", "MP3 / AAC のオプション...", "خيارات MP3 / AAC...", "אפשרויות MP3 / AAC..."),
    "Start the scan as soon as this window opens on a file": (
        "Iniciar el análisis en cuanto esta ventana se abra con un archivo", "Avvia la scansione appena questa finestra si apre su un file",
        "Lancer l'analyse dès que cette fenêtre s'ouvre sur un fichier", "Durchsuchen starten, sobald sich dieses Fenster mit einer Datei öffnet",
        "Rozpocznij skanowanie, gdy tylko okno otworzy się z plikiem", "Начинать сканирование, как только окно открыто с файлом",
        "窗口打开并指定文件后立即开始扫描", "ウィンドウがファイル付きで開いたらすぐにスキャンを開始する", "ابدأ الفحص بمجرد فتح هذه النافذة على ملف", "התחל בסריקה מיד כשהחלון נפתח עם קובץ"),
    "Open the files found as soon as the scan ends": (
        "Abrir los archivos encontrados en cuanto termine el análisis", "Apri i file trovati appena termina la scansione",
        "Ouvrir les fichiers trouvés dès la fin de l'analyse", "Gefundene Dateien öffnen, sobald die Suche endet",
        "Otwórz znalezione pliki zaraz po zakończeniu skanowania", "Открыть найденные файлы сразу после окончания сканирования",
        "扫描结束后立即打开找到的文件", "スキャンが終わったらすぐに見つかったファイルを開く", "افتح الملفات التي تم العثور عليها فور انتهاء الفحص", "פתח את הקבצים שנמצאו מיד בסיום הסריקה"),
    "Name the files with this template:": (
        "Nombrar los archivos con esta plantilla:", "Assegna ai file un nome con questo modello:", "Nommer les fichiers avec ce modèle :",
        "Dateien nach dieser Vorlage benennen:", "Nazywaj pliki według tego szablonu:", "Называть файлы по этому шаблону:",
        "使用以下模板命名文件：", "次のテンプレートでファイル名を付ける：", "تسمية الملفات بهذا القالب:", "תן שמות לקבצים לפי התבנית הזו:"),
    "Put the files in the folders Audio, Video, Image and Unknown": (
        "Poner los archivos en las carpetas Audio, Vídeo, Imagen y Desconocido", "Metti i file nelle cartelle Audio, Video, Immagini e Sconosciuti",
        "Placer les fichiers dans les dossiers Audio, Vidéo, Image et Inconnu", "Dateien in die Ordner Audio, Video, Bild und Unbekannt legen",
        "Umieszczaj pliki w folderach Audio, Wideo, Obraz i Nieznane", "Помещать файлы в папки «Аудио», «Видео», «Изображения» и «Неизвестные»",
        "将文件放入“音频”“视频”“图像”和“未知”文件夹", "ファイルを「オーディオ」「ビデオ」「画像」「不明」フォルダーに入れる", "وضع الملفات في المجلدات صوت وفيديو وصورة ومجهول", "הכנס את הקבצים לתיקיות שמע, וידאו, תמונה ולא ידוע"),
    "Save an HRF file (to open the finds again without scanning)": (
        "Guardar un archivo HRF (para reabrir lo encontrado sin analizar de nuevo)", "Salva un file HRF (per riaprire i risultati senza scansionare di nuovo)",
        "Enregistrer un fichier HRF (pour rouvrir les résultats sans réanalyser)", "HRF-Datei speichern (um die Funde ohne erneutes Durchsuchen zu öffnen)",
        "Zapisz plik HRF (aby ponownie otworzyć wyniki bez skanowania)", "Сохранить файл HRF (чтобы открывать найденное без повторного сканирования)",
        "保存 HRF 文件（无需重新扫描即可再次打开结果）", "HRF ファイルを保存する（再スキャンせずに結果を開き直せます）", "حفظ ملف HRF (لإعادة فتح النتائج دون فحص جديد)", "שמור קובץ HRF (כדי לפתוח שוב את התוצאות בלי לסרוק מחדש)"),
    "Add information:": (
        "Añadir información:", "Aggiungi informazioni:", "Ajouter des informations :", "Informationen hinzufügen:", "Dodaj informacje:",
        "Добавить сведения:", "添加信息：", "情報を追加する：", "إضافة معلومات:", "הוסף מידע:"),
    "Do not store the program and its version (version 3)": (
        "No guardar el programa ni su versión (versión 3)", "Non memorizzare il programma e la sua versione (versione 3)",
        "Ne pas enregistrer le programme et sa version (version 3)", "Programm und Version nicht speichern (Version 3)",
        "Nie zapisuj programu i jego wersji (wersja 3)", "Не сохранять программу и её версию (версия 3)",
        "不保存程序及其版本（版本 3）", "プログラムとそのバージョンを保存しない（バージョン 3）", "عدم تخزين البرنامج وإصداره (الإصدار 3)", "אל תשמור את התוכנית ואת גרסתה (גרסה 3)"),
    "Browse...": (
        "Examinar...", "Sfoglia...", "Parcourir...", "Durchsuchen ...", "Przeglądaj...", "Обзор...", "浏览...", "参照...", "استعراض...", "עיון..."),
    "All": ("Todos", "Tutti", "Tous", "Alle", "Wszystkie", "Все", "全部", "すべて", "الكل", "הכול"),
    "Images": ("Imágenes", "Immagini", "Images", "Bilder", "Obrazy", "Изображения", "图像", "画像", "الصور", "תמונות"),
    "Videos": ("Vídeos", "Video", "Vidéos", "Videos", "Filmy", "Видео", "视频", "動画", "مقاطع الفيديو", "סרטונים"),
    "Audio": ("Audio", "Audio", "Audio", "Audio", "Audio", "Аудио", "音频", "オーディオ", "الصوت", "שמע"),
    "Formats": ("Formatos", "Formati", "Formats", "Formate", "Formaty", "Форматы", "格式", "形式", "الصيغ", "פורמטים"),
    "%f name of the scanned file, %x its extension, %o offset, %h offset in hexadecimal, %s size, %l size in hexadecimal, %n number of the find. The default is %f_%x-%n.": (
        "%f nombre del archivo analizado, %x su extensión, %o desplazamiento, %h desplazamiento en hexadecimal, %s tamaño, %l tamaño en hexadecimal, %n número del hallazgo. Por defecto es %f_%x-%n.",
        "%f nome del file scansionato, %x la sua estensione, %o offset, %h offset in esadecimale, %s dimensione, %l dimensione in esadecimale, %n numero del risultato. Il valore predefinito è %f_%x-%n.",
        "%f nom du fichier analysé, %x son extension, %o décalage, %h décalage en hexadécimal, %s taille, %l taille en hexadécimal, %n numéro du résultat. La valeur par défaut est %f_%x-%n.",
        "%f Name der durchsuchten Datei, %x ihre Erweiterung, %o Offset, %h Offset hexadezimal, %s Größe, %l Größe hexadezimal, %n Nummer des Fundes. Standard ist %f_%x-%n.",
        "%f nazwa skanowanego pliku, %x jego rozszerzenie, %o przesunięcie, %h przesunięcie szesnastkowo, %s rozmiar, %l rozmiar szesnastkowo, %n numer znaleziska. Domyślnie %f_%x-%n.",
        "%f имя сканируемого файла, %x его расширение, %o смещение, %h смещение в шестнадцатеричном виде, %s размер, %l размер в шестнадцатеричном виде, %n номер находки. По умолчанию %f_%x-%n.",
        "%f 被扫描文件的名称，%x 其扩展名，%o 偏移，%h 十六进制偏移，%s 大小，%l 十六进制大小，%n 结果编号。默认为 %f_%x-%n。",
        "%f スキャンしたファイルの名前、%x その拡張子、%o オフセット、%h 16 進数のオフセット、%s サイズ、%l 16 進数のサイズ、%n 検出番号。既定は %f_%x-%n です。",
        "%f اسم الملف الذي تم فحصه، %x امتداده، %o الإزاحة، %h الإزاحة بالنظام السداسي عشري، %s الحجم، %l الحجم بالنظام السداسي عشري، %n رقم النتيجة. القيمة الافتراضية هي %f_%x-%n.",
        "%f שם הקובץ שנסרק, %x הסיומת שלו, %o היסט, %h היסט בהקסדצימלי, %s גודל, %l גודל בהקסדצימלי, %n מספר התוצאה. ברירת המחדל היא %f_%x-%n."),
    "Names": ("Nombres", "Nomi", "Noms", "Namen", "Nazwy", "Имена", "名称", "名前", "الأسماء", "שמות"),
    "File": ("Archivo", "File", "Fichier", "Datei", "Plik", "Файл", "文件", "ファイル", "الملف", "קובץ"),
    "Version 3.0 (64 bits offsets, long names)": (
        "Versión 3.0 (desplazamientos de 64 bits, nombres largos)", "Versione 3.0 (offset a 64 bit, nomi lunghi)",
        "Version 3.0 (décalages 64 bits, noms longs)", "Version 3.0 (64-Bit-Offsets, lange Namen)", "Wersja 3.0 (przesunięcia 64-bitowe, długie nazwy)",
        "Версия 3.0 (64-битные смещения, длинные имена)", "版本 3.0（64 位偏移，长名称）", "バージョン 3.0（64 ビットオフセット、長い名前）", "الإصدار 3.0 (إزاحات 64 بت، أسماء طويلة)", "גרסה 3.0 (היסטים של 64 סיביות, שמות ארוכים)"),
    "Version 2 (names of 63 characters)": (
        "Versión 2 (nombres de 63 caracteres)", "Versione 2 (nomi di 63 caratteri)", "Version 2 (noms de 63 caractères)", "Version 2 (Namen mit 63 Zeichen)",
        "Wersja 2 (nazwy do 63 znaków)", "Версия 2 (имена до 63 символов)", "版本 2（名称最多 63 个字符）", "バージョン 2（名前は 63 文字まで）", "الإصدار 2 (أسماء من 63 حرفاً)", "גרסה 2 (שמות של 63 תווים)"),
    "Version 1 (names of 31 characters)": (
        "Versión 1 (nombres de 31 caracteres)", "Versione 1 (nomi di 31 caratteri)", "Version 1 (noms de 31 caractères)", "Version 1 (Namen mit 31 Zeichen)",
        "Wersja 1 (nazwy do 31 znaków)", "Версия 1 (имена до 31 символа)", "版本 1（名称最多 31 个字符）", "バージョン 1（名前は 31 文字まで）", "الإصدار 1 (أسماء من 31 حرفاً)", "גרסה 1 (שמות של 31 תווים)"),
    "Format": ("Formato", "Formato", "Format", "Format", "Format", "Формат", "格式", "形式", "الصيغة", "פורמט"),
    "Title": ("Título", "Titolo", "Titre", "Titel", "Tytuł", "Название", "标题", "タイトル", "العنوان", "כותרת"),
    "Author": ("Autor", "Autore", "Auteur", "Autor", "Autor", "Автор", "作者", "作成者", "المؤلف", "מחבר"),
    "URL": ("URL",) * 10,
    "HRF file": ("Archivo HRF", "File HRF", "Fichier HRF", "HRF-Datei", "Plik HRF", "Файл HRF", "HRF 文件", "HRF ファイル", "ملف HRF", "קובץ HRF"),
    " bytes": (" bytes", " byte", " octets", " Bytes", " bajtów", " байт", " 字节", " バイト", " بايت", " בייטים"),
    "Ignore finds smaller than": (
        "Ignorar hallazgos menores de", "Ignora i risultati più piccoli di", "Ignorer les résultats plus petits que", "Funde ignorieren, die kleiner sind als",
        "Ignoruj znaleziska mniejsze niż", "Игнорировать находки меньше", "忽略小于以下大小的结果", "次より小さい検出結果を無視する", "تجاهل النتائج الأصغر من", "התעלם מתוצאות קטנות מ־"),
    " KiB": (" KiB",) * 10,
    "What is read from the file at once: only the speed changes, not the finds": (
        "Lo que se lee del archivo de una vez: solo cambia la velocidad, no los hallazgos", "Quanto viene letto dal file in una volta: cambia solo la velocità, non i risultati",
        "Ce qui est lu du fichier en une fois : seule la vitesse change, pas les résultats", "Wie viel auf einmal aus der Datei gelesen wird: nur die Geschwindigkeit ändert sich, nicht die Funde",
        "Ilość odczytywana z pliku naraz: zmienia się tylko szybkość, nie wyniki", "Сколько читается из файла за один раз: меняется только скорость, но не результаты",
        "一次从文件读取的数据量：只影响速度，不影响结果", "一度にファイルから読み込む量：速度だけが変わり、検出結果は変わりません", "المقدار المقروء من الملف دفعة واحدة: تتغير السرعة فقط وليس النتائج", "כמה נקרא מהקובץ בבת אחת: רק המהירות משתנה, לא התוצאות"),
    "Read buffer": ("Búfer de lectura", "Buffer di lettura", "Tampon de lecture", "Lesepuffer", "Bufor odczytu", "Буфер чтения", "读取缓冲区", "読み取りバッファー", "مخزن القراءة المؤقت", "מאגר קריאה"),
    "Options": ("Opciones", "Opzioni", "Options", "Optionen", "Opcje", "Параметры", "选项", "オプション", "الخيارات", "אפשרויות"),
    "Close": ("Cerrar", "Chiudi", "Fermer", "Schließen", "Zamknij", "Закрыть", "关闭", "閉じる", "إغلاق", "סגור"),
    "HyperRipper files (*.hrf)": (
        "Archivos HyperRipper (*.hrf)", "File HyperRipper (*.hrf)", "Fichiers HyperRipper (*.hrf)", "HyperRipper-Dateien (*.hrf)", "Pliki HyperRipper (*.hrf)",
        "Файлы HyperRipper (*.hrf)", "HyperRipper 文件 (*.hrf)", "HyperRipper ファイル (*.hrf)", "ملفات HyperRipper (*.hrf)", "קובצי HyperRipper (*.hrf)"),
    "File to scan": ("Archivo que analizar", "File da scansionare", "Fichier à analyser", "Zu durchsuchende Datei", "Plik do przeskanowania", "Файл для сканирования", "要扫描的文件", "スキャンするファイル", "الملف المراد فحصه", "קובץ לסריקה"),
    "Cancel": ("Cancelar", "Annulla", "Annuler", "Abbrechen", "Anuluj", "Отмена", "取消", "キャンセル", "إلغاء", "ביטול"),
    "Select at least one type": (
        "Seleccione al menos un tipo", "Seleziona almeno un tipo", "Sélectionnez au moins un type", "Wählen Sie mindestens einen Typ aus", "Wybierz co najmniej jeden typ",
        "Выберите хотя бы один тип", "请至少选择一种类型", "少なくとも 1 つの種類を選択してください", "اختر نوعاً واحداً على الأقل", "בחרו לפחות סוג אחד"),
    "Speed %1/s    Scanned %2    Left %3    Elapsed %4    Remaining %5": (
        "Velocidad %1/s    Analizado %2    Falta %3    Transcurrido %4    Restante %5", "Velocità %1/s    Scansionato %2    Rimanente %3    Trascorso %4    Tempo residuo %5",
        "Vitesse %1/s    Analysé %2    Reste %3    Écoulé %4    Restant %5", "Geschwindigkeit %1/s    Durchsucht %2    Übrig %3    Vergangen %4    Verbleibend %5",
        "Szybkość %1/s    Przeskanowano %2    Pozostało %3    Upłynęło %4    Pozostały czas %5", "Скорость %1/с    Просканировано %2    Осталось %3    Прошло %4    Ожидание %5",
        "速度 %1/秒    已扫描 %2    剩余 %3    已用时间 %4    预计剩余 %5", "速度 %1/秒    スキャン済み %2    残り %3    経過 %4    推定残り時間 %5",
        "السرعة %1/ث    تم فحص %2    المتبقي %3    المنقضي %4    الوقت المتبقي %5", "מהירות %1/ש׳    נסרק %2    נותר %3    חלף %4    זמן משוער %5"),
    "Scanning: %1 of %2": (
        "Analizando: %1 de %2", "Scansione: %1 di %2", "Analyse : %1 sur %2", "Durchsuche: %1 von %2", "Skanowanie: %1 z %2", "Сканирование: %1 из %2", "正在扫描：%1 / %2", "スキャン中：%1 / %2", "جارٍ الفحص: %1 من %2", "סורק: %1 מתוך %2"),
    "Cannot scan: %1": (
        "No se puede analizar: %1", "Impossibile scansionare: %1", "Impossible d'analyser : %1", "Durchsuchen nicht möglich: %1", "Nie można przeskanować: %1", "Не удалось просканировать: %1",
        "无法扫描：%1", "スキャンできません：%1", "تعذّر الفحص: %1", "לא ניתן לסרוק: %1"),
    "Cancelled": ("Cancelado", "Annullato", "Annulé", "Abgebrochen", "Anulowano", "Отменено", "已取消", "キャンセルしました", "تم الإلغاء", "בוטל"),
    ", saved in %1": (
        ", guardado en %1", ", salvato in %1", ", enregistré dans %1", ", gespeichert in %1", ", zapisano w %1", ", сохранено в %1", "，已保存到 %1", "、%1 に保存しました", "، تم الحفظ في %1", ", נשמר ב־%1"),
    ", but the HRF file was not saved: %1": (
        ", pero el archivo HRF no se guardó: %1", ", ma il file HRF non è stato salvato: %1", ", mais le fichier HRF n'a pas été enregistré : %1", ", die HRF-Datei wurde jedoch nicht gespeichert: %1",
        ", ale plik HRF nie został zapisany: %1", ", но файл HRF не сохранён: %1", "，但 HRF 文件未能保存：%1", "、ただし HRF ファイルは保存されませんでした：%1", "، لكن لم يتم حفظ ملف HRF: %1", ", אך קובץ ה־HRF לא נשמר: %1"),
    "%1 files found": (
        "%1 archivos encontrados", "%1 file trovati", "%1 fichiers trouvés", "%1 Dateien gefunden", "Znaleziono plików: %1", "Найдено файлов: %1", "找到 %1 个文件", "%1 個のファイルが見つかりました", "تم العثور على %1 ملفات", "נמצאו %1 קבצים"),
    "Nothing found": ("No se encontró nada", "Nessun risultato", "Rien trouvé", "Nichts gefunden", "Nic nie znaleziono", "Ничего не найдено", "未找到任何内容", "何も見つかりませんでした", "لم يتم العثور على شيء", "לא נמצא דבר"),
    "Select an image to preview it": (
        "Seleccione una imagen para verla", "Seleziona un'immagine per vederne l'anteprima", "Sélectionnez une image pour l'afficher", "Wählen Sie ein Bild für die Vorschau aus", "Wybierz obraz, aby go podejrzeć",
        "Выберите изображение для просмотра", "选择图像以预览", "プレビューする画像を選択してください", "اختر صورة لمعاينتها", "בחרו תמונה כדי לראות אותה בתצוגה מקדימה"),
    "%1: %2 extracted, %3 skipped, %4 failed": (
        "%1: %2 extraídos, %3 omitidos, %4 con error", "%1: %2 estratti, %3 saltati, %4 non riusciti", "%1 : %2 extraits, %3 ignorés, %4 en échec", "%1: %2 extrahiert, %3 übersprungen, %4 fehlgeschlagen",
        "%1: wypakowano %2, pominięto %3, błędów %4", "%1: извлечено %2, пропущено %3, с ошибкой %4", "%1：已提取 %2 个，跳过 %3 个，失败 %4 个", "%1：%2 件を展開、%3 件をスキップ、%4 件が失敗",
        "%1: تم استخراج %2، وتخطي %3، وفشل %4", "%1: חולצו %2, דולגו %3, נכשלו %4"),
    "%1: cancelled (%2 extracted)": (
        "%1: cancelado (%2 extraídos)", "%1: annullato (%2 estratti)", "%1 : annulé (%2 extraits)", "%1: abgebrochen (%2 extrahiert)", "%1: anulowano (wypakowano %2)", "%1: отменено (извлечено %2)",
        "%1：已取消（已提取 %2 个）", "%1：キャンセルしました（%2 件を展開済み）", "%1: تم الإلغاء (تم استخراج %2)", "%1: בוטל (חולצו %2)"),
    "%1: failed: %2": (
        "%1: error: %2", "%1: non riuscito: %2", "%1 : échec : %2", "%1: fehlgeschlagen: %2", "%1: niepowodzenie: %2", "%1: ошибка: %2", "%1：失败：%2", "%1：失敗しました：%2", "%1: فشل: %2", "%1: נכשל: %2"),
    "%1 of %2  %3": ("%1 de %2  %3", "%1 di %2  %3", "%1 sur %2  %3", "%1 von %2  %3", "%1 z %2  %3", "%1 из %2  %3", "%1 / %2  %3", "%1 / %2  %3", "%1 من %2  %3", "%1 מתוך %2  %3"),
    "Dismiss": ("Descartar", "Chiudi", "Fermer", "Verwerfen", "Odrzuć", "Скрыть", "忽略", "閉じる", "تجاهل", "הסר"),
    "Done: %1 extracted, %2 skipped, %3 failed": (
        "Terminado: %1 extraídos, %2 omitidos, %3 con error", "Completato: %1 estratti, %2 saltati, %3 non riusciti", "Terminé : %1 extraits, %2 ignorés, %3 en échec",
        "Fertig: %1 extrahiert, %2 übersprungen, %3 fehlgeschlagen", "Gotowe: wypakowano %1, pominięto %2, błędów %3", "Готово: извлечено %1, пропущено %2, с ошибкой %3",
        "完成：已提取 %1 个，跳过 %2 个，失败 %3 个", "完了：%1 件を展開、%2 件をスキップ、%3 件が失敗", "تم: استخراج %1، وتخطي %2، وفشل %3", "הושלם: חולצו %1, דולגו %2, נכשלו %3"),
    "Failed: %1": ("Error: %1", "Non riuscito: %1", "Échec : %1", "Fehlgeschlagen: %1", "Niepowodzenie: %1", "Ошибка: %1", "失败：%1", "失敗：%1", "فشل: %1", "נכשל: %1"),
    "Filter by name (Ctrl+F)": (
        "Filtrar por nombre (Ctrl+F)", "Filtra per nome (Ctrl+F)", "Filtrer par nom (Ctrl+F)", "Nach Namen filtern (Strg+F)", "Filtruj według nazwy (Ctrl+F)", "Фильтр по имени (Ctrl+F)",
        "按名称筛选 (Ctrl+F)", "名前で絞り込み (Ctrl+F)", "تصفية حسب الاسم (Ctrl+F)", "סינון לפי שם (Ctrl+F)"),
    "Whole archive": (
        "Todo el archivo", "Intero archivio", "Toute l'archive", "Gesamtes Archiv", "Całe archiwum", "Весь архив", "整个归档", "アーカイブ全体", "الأرشيف بالكامل", "כל הארכיון"),
    "Search in every folder (Ctrl+Shift+F)": (
        "Buscar en todas las carpetas (Ctrl+Mayús+F)", "Cerca in tutte le cartelle (Ctrl+Maiusc+F)", "Rechercher dans tous les dossiers (Ctrl+Maj+F)", "In allen Ordnern suchen (Strg+Umschalt+F)",
        "Szukaj we wszystkich folderach (Ctrl+Shift+F)", "Искать во всех папках (Ctrl+Shift+F)", "在所有文件夹中搜索 (Ctrl+Shift+F)", "すべてのフォルダーを検索 (Ctrl+Shift+F)",
        "البحث في كل المجلدات (Ctrl+Shift+F)", "חיפוש בכל התיקיות (Ctrl+Shift+F)"),
    "%1 entries": ("%1 entradas", "%1 voci", "%1 entrées", "%1 Einträge", "%1 pozycji", "Записей: %1", "%1 个条目", "%1 エントリ", "%1 عنصر", "%1 פריטים"),
    "Thumbnail size": (
        "Tamaño de las miniaturas", "Dimensione delle miniature", "Taille des miniatures", "Größe der Miniaturansichten", "Rozmiar miniatur", "Размер миниатюр", "缩略图大小", "サムネイルのサイズ", "حجم الصور المصغرة", "גודל תמונות ממוזערות"),
    "Extract selected...": (
        "Extraer seleccionados...", "Estrai selezionati...", "Extraire la sélection...", "Auswahl extrahieren ...", "Wypakuj zaznaczone...", "Извлечь выбранное...", "提取所选项...", "選択項目を展開...", "استخراج المحدد...", "חלץ את הנבחרים..."),
    "Extract all...": (
        "Extraer todo...", "Estrai tutto...", "Tout extraire...", "Alles extrahieren ...", "Wypakuj wszystko...", "Извлечь всё...", "全部提取...", "すべて展開...", "استخراج الكل...", "חלץ הכול..."),
    "Extract selected, images as PNG...": (
        "Extraer seleccionados, imágenes como PNG...", "Estrai selezionati, immagini come PNG...", "Extraire la sélection, images en PNG...", "Auswahl extrahieren, Bilder als PNG ...",
        "Wypakuj zaznaczone, obrazy jako PNG...", "Извлечь выбранное, изображения как PNG...", "提取所选项，图像保存为 PNG...", "選択項目を展開（画像は PNG）...", "استخراج المحدد، الصور بصيغة PNG...", "חלץ את הנבחרים, תמונות כ־PNG..."),
    "Extract this folder...": (
        "Extraer esta carpeta...", "Estrai questa cartella...", "Extraire ce dossier...", "Diesen Ordner extrahieren ...", "Wypakuj ten folder...", "Извлечь эту папку...", "提取此文件夹...", "このフォルダーを展開...", "استخراج هذا المجلد...", "חלץ את התיקייה הזו..."),
    "Driver: %1 (%2 formats)": (
        "Controlador: %1 (%2 formatos)", "Driver: %1 (%2 formati)", "Pilote : %1 (%2 formats)", "Treiber: %1 (%2 Formate)", "Sterownik: %1 (formatów: %2)", "Драйвер: %1 (форматов: %2)",
        "驱动程序：%1（%2 种格式）", "ドライバー：%1（%2 形式）", "برنامج التشغيل: %1 (%2 صيغة)", "מנהל: %1 (%2 פורמטים)"),
    "Driver": ("Controlador", "Driver", "Pilote", "Treiber", "Sterownik", "Драйвер", "驱动程序", "ドライバー", "برنامج التشغيل", "מנהל פורמטים"),
    "Cannot load the driver:\n%1\n%2": (
        "No se puede cargar el controlador:\n%1\n%2", "Impossibile caricare il driver:\n%1\n%2", "Impossible de charger le pilote :\n%1\n%2", "Der Treiber kann nicht geladen werden:\n%1\n%2",
        "Nie można załadować sterownika:\n%1\n%2", "Не удалось загрузить драйвер:\n%1\n%2", "无法加载驱动程序：\n%1\n%2", "ドライバーを読み込めません：\n%1\n%2", "تعذّر تحميل برنامج التشغيل:\n%1\n%2", "לא ניתן לטעון את המנהל:\n%1\n%2"),
    "File": ("Archivo", "File", "Fichier", "Datei", "Plik", "Файл", "文件", "ファイル", "الملف", "קובץ"),
    "Open...": ("Abrir...", "Apri...", "Ouvrir...", "Öffnen ...", "Otwórz...", "Открыть...", "打开...", "開く...", "فتح...", "פתח..."),
    "Open recent": ("Abrir reciente", "Apri recenti", "Ouvrir un fichier récent", "Zuletzt geöffnet", "Otwórz ostatnie", "Открыть недавние", "打开最近的文件", "最近使用したファイルを開く", "فتح الأخيرة", "פתח אחרונים"),
    "Extract selected, converted with...": (
        "Extraer seleccionados, convertidos con...", "Estrai selezionati, convertiti con...", "Extraire la sélection, convertie avec...", "Auswahl extrahieren, konvertiert mit ...",
        "Wypakuj zaznaczone, przekonwertowane za pomocą...", "Извлечь выбранное, преобразовав с помощью...", "提取所选项，并使用以下工具转换...", "選択項目を展開し、次で変換...", "استخراج المحدد وتحويله باستخدام...", "חלץ את הנבחרים, מומרים באמצעות..."),
    "Extract all, converted with...": (
        "Extraer todo, convertido con...", "Estrai tutto, convertito con...", "Tout extraire, converti avec...", "Alles extrahieren, konvertiert mit ...", "Wypakuj wszystko, przekonwertowane za pomocą...",
        "Извлечь всё, преобразовав с помощью...", "全部提取，并使用以下工具转换...", "すべて展開し、次で変換...", "استخراج الكل وتحويله باستخدام...", "חלץ הכול, מומרים באמצעות..."),
    "Extract all, images as PNG...": (
        "Extraer todo, imágenes como PNG...", "Estrai tutto, immagini come PNG...", "Tout extraire, images en PNG...", "Alles extrahieren, Bilder als PNG ...", "Wypakuj wszystko, obrazy jako PNG...",
        "Извлечь всё, изображения как PNG...", "全部提取，图像保存为 PNG...", "すべて展開（画像は PNG）...", "استخراج الكل، الصور بصيغة PNG...", "חלץ הכול, תמונות כ־PNG..."),
    "Export the list of entries...": (
        "Exportar la lista de entradas...", "Esporta l'elenco delle voci...", "Exporter la liste des entrées...", "Liste der Einträge exportieren ...", "Eksportuj listę pozycji...",
        "Экспортировать список записей...", "导出条目列表...", "エントリの一覧をエクスポート...", "تصدير قائمة العناصر...", "ייצא את רשימת הפריטים..."),
    "Quit": ("Salir", "Esci", "Quitter", "Beenden", "Zakończ", "Выход", "退出", "終了", "خروج", "יציאה"),
    "View": ("Ver", "Visualizza", "Affichage", "Ansicht", "Widok", "Вид", "查看", "表示", "عرض", "תצוגה"),
    "Include subfolders": (
        "Incluir subcarpetas", "Includi sottocartelle", "Inclure les sous-dossiers", "Unterordner einbeziehen", "Uwzględnij podfoldery", "Включая вложенные папки", "包含子文件夹", "サブフォルダーを含める", "تضمين المجلدات الفرعية", "כלול תיקיות משנה"),
    "Details": ("Detalles", "Dettagli", "Détails", "Details", "Szczegóły", "Таблица", "详细信息", "詳細", "التفاصيل", "פרטים"),
    "Thumbnails": ("Miniaturas", "Miniature", "Miniatures", "Miniaturansichten", "Miniatury", "Миниатюры", "缩略图", "サムネイル", "الصور المصغرة", "תמונות ממוזערות"),
    "Back": ("Atrás", "Indietro", "Précédent", "Zurück", "Wstecz", "Назад", "后退", "戻る", "رجوع", "אחורה"),
    "Forward": ("Adelante", "Avanti", "Suivant", "Vor", "Dalej", "Вперёд", "前进", "進む", "للأمام", "קדימה"),
    "Up one folder": (
        "Subir una carpeta", "Cartella superiore", "Dossier parent", "Einen Ordner nach oben", "Folder wyżej", "На уровень вверх", "上一级文件夹", "1 つ上のフォルダー", "المجلد الأعلى", "תיקייה אחת למעלה"),
    "Theme": ("Tema", "Tema", "Thème", "Design", "Motyw", "Тема", "主题", "テーマ", "السمة", "ערכת נושא"),
    "Dark": ("Oscuro", "Scuro", "Sombre", "Dunkel", "Ciemny", "Тёмная", "深色", "ダーク", "داكن", "כהה"),
    "Light": ("Claro", "Chiaro", "Clair", "Hell", "Jasny", "Светлая", "浅色", "ライト", "فاتح", "בהיר"),
    "Tools": ("Herramientas", "Strumenti", "Outils", "Extras", "Narzędzia", "Инструменты", "工具", "ツール", "أدوات", "כלים"),
    "HyperRipper...": ("HyperRipper...",) * 10,
    "Options...": ("Opciones...", "Opzioni...", "Options...", "Optionen ...", "Opcje...", "Параметры...", "选项...", "オプション...", "الخيارات...", "אפשרויות..."),
    "Help": ("Ayuda", "Aiuto", "Aide", "Hilfe", "Pomoc", "Справка", "帮助", "ヘルプ", "مساعدة", "עזרה"),
    "About": ("Acerca de", "Informazioni", "À propos", "Info", "O programie", "О программе", "关于", "バージョン情報", "حول", "אודות"),
    "Dragon UnPACKer 6 - Qt interface (work in progress).": (
        "Dragon UnPACKer 6 - interfaz Qt (en desarrollo).", "Dragon UnPACKer 6 - interfaccia Qt (in sviluppo).", "Dragon UnPACKer 6 - interface Qt (en cours de développement).",
        "Dragon UnPACKer 6 – Qt-Oberfläche (in Entwicklung).", "Dragon UnPACKer 6 – interfejs Qt (w trakcie prac).", "Dragon UnPACKer 6 — интерфейс на Qt (в разработке).",
        "Dragon UnPACKer 6 - Qt 界面（开发中）。", "Dragon UnPACKer 6 - Qt インターフェイス（開発中）。", "Dragon UnPACKer 6 - واجهة Qt (قيد التطوير).", "Dragon UnPACKer 6 - ממשק Qt (בפיתוח)."),
    "\n\nDriver: %1, %2 formats\n%3": (
        "\n\nControlador: %1, %2 formatos\n%3", "\n\nDriver: %1, %2 formati\n%3", "\n\nPilote : %1, %2 formats\n%3", "\n\nTreiber: %1, %2 Formate\n%3", "\n\nSterownik: %1, formatów: %2\n%3",
        "\n\nДрайвер: %1, форматов: %2\n%3", "\n\n驱动程序：%1，%2 种格式\n%3", "\n\nドライバー：%1、%2 形式\n%3", "\n\nبرنامج التشغيل: %1، %2 صيغة\n%3", "\n\nמנהל: %1, %2 פורמטים\n%3"),
    "Main": ("Principal", "Principale", "Principal", "Hauptfenster", "Główne", "Основное", "主要", "メイン", "الرئيسية", "ראשי"),
    "Hierarchy": ("Jerarquía", "Gerarchia", "Hiérarchie", "Hierarchie", "Hierarchia", "Иерархия", "层次结构", "階層", "التسلسل الهرمي", "היררכיה"),
    "Viewer": ("Visor", "Visualizzatore", "Visionneuse", "Betrachter", "Podgląd", "Просмотр", "查看器", "ビューアー", "العارض", "מציג"),
    "Inspector": ("Inspector", "Ispettore", "Inspecteur", "Inspektor", "Inspektor", "Инспектор", "检查器", "インスペクター", "المفتش", "בודק"),
    "Console": ("Consola", "Console", "Console", "Konsole", "Konsola", "Консоль", "控制台", "コンソール", "وحدة التحكم", "מסוף"),
    "Jobs": ("Tareas", "Attività", "Tâches", "Aufgaben", "Zadania", "Задачи", "任务", "ジョブ", "المهام", "משימות"),
    "Workspace": ("Espacio de trabajo", "Area di lavoro", "Espace de travail", "Arbeitsbereich", "Obszar roboczy", "Рабочая область", "工作区", "ワークスペース", "مساحة العمل", "סביבת עבודה"),
    "Default": ("Predeterminado", "Predefinito", "Par défaut", "Standard", "Domyślny", "По умолчанию", "默认", "既定", "الافتراضي", "ברירת מחדל"),
    "Browse (folders and entries)": (
        "Examinar (carpetas y entradas)", "Esplora (cartelle e voci)", "Parcourir (dossiers et entrées)", "Durchsuchen (Ordner und Einträge)", "Przeglądanie (foldery i pozycje)", "Обзор (папки и записи)",
        "浏览（文件夹和条目）", "参照（フォルダーとエントリ）", "استعراض (المجلدات والعناصر)", "עיון (תיקיות ופריטים)"),
    "Preview (entries, viewer, inspector)": (
        "Vista previa (entradas, visor, inspector)", "Anteprima (voci, visualizzatore, ispettore)", "Aperçu (entrées, visionneuse, inspecteur)", "Vorschau (Einträge, Betrachter, Inspektor)",
        "Podgląd (pozycje, podgląd, inspektor)", "Просмотр (записи, просмотр, инспектор)", "预览（条目、查看器、检查器）", "プレビュー（エントリ、ビューアー、インスペクター）", "معاينة (العناصر، العارض، المفتش)", "תצוגה מקדימה (פריטים, מציג, בודק)"),
    "All supported formats (%1)": (
        "Todos los formatos compatibles (%1)", "Tutti i formati supportati (%1)", "Tous les formats pris en charge (%1)", "Alle unterstützten Formate (%1)", "Wszystkie obsługiwane formaty (%1)",
        "Все поддерживаемые форматы (%1)", "所有支持的格式 (%1)", "サポートされているすべての形式 (%1)", "كل الصيغ المدعومة (%1)", "כל הפורמטים הנתמכים (%1)"),
    "All files (*)": ("Todos los archivos (*)", "Tutti i file (*)", "Tous les fichiers (*)", "Alle Dateien (*)", "Wszystkie pliki (*)", "Все файлы (*)", "所有文件 (*)", "すべてのファイル (*)", "كل الملفات (*)", "כל הקבצים (*)"),
    "Cannot scan %1: %2": (
        "No se puede analizar %1: %2", "Impossibile scansionare %1: %2", "Impossible d'analyser %1 : %2", "%1 kann nicht durchsucht werden: %2", "Nie można przeskanować %1: %2", "Не удалось просканировать %1: %2",
        "无法扫描 %1：%2", "%1 をスキャンできません：%2", "تعذّر فحص %1: %2", "לא ניתן לסרוק את %1: %2"),
    "Where is %1?": ("¿Dónde está %1?", "Dov'è %1?", "Où se trouve %1 ?", "Wo befindet sich %1?", "Gdzie jest %1?", "Где находится %1?", "%1 在哪里？", "%1 はどこにありますか？", "أين يوجد %1؟", "היכן נמצא %1?"),
    "%1 is not the file the HRF file was made for (%2 bytes instead of %3)": (
        "%1 no es el archivo para el que se creó el archivo HRF (%2 bytes en lugar de %3)", "%1 non è il file per cui è stato creato il file HRF (%2 byte invece di %3)",
        "%1 n'est pas le fichier pour lequel le fichier HRF a été créé (%2 octets au lieu de %3)", "%1 ist nicht die Datei, für die die HRF-Datei erstellt wurde (%2 statt %3 Bytes)",
        "%1 nie jest plikiem, dla którego utworzono plik HRF (%2 bajtów zamiast %3)", "%1 — не тот файл, для которого создан файл HRF (%2 байт вместо %3)",
        "%1 不是该 HRF 文件所对应的文件（%2 字节，应为 %3 字节）", "%1 は、この HRF ファイルの対象ファイルではありません（%3 バイトのはずが %2 バイト）",
        "%1 ليس الملف الذي أُنشئ له ملف HRF (%2 بايت بدلاً من %3)", "%1 אינו הקובץ שעבורו נוצר קובץ ה־HRF (%2 בייטים במקום %3)"),
    "The HRF file is damaged: %1 is outside of the file": (
        "El archivo HRF está dañado: %1 está fuera del archivo", "Il file HRF è danneggiato: %1 è fuori dal file", "Le fichier HRF est endommagé : %1 est en dehors du fichier",
        "Die HRF-Datei ist beschädigt: %1 liegt außerhalb der Datei", "Plik HRF jest uszkodzony: %1 leży poza plikiem", "Файл HRF повреждён: %1 находится за пределами файла",
        "HRF 文件已损坏：%1 超出了文件范围", "HRF ファイルが壊れています：%1 はファイルの範囲外です", "ملف HRF تالف: %1 خارج حدود الملف", "קובץ ה־HRF פגום: %1 נמצא מחוץ לקובץ"),
    "Clear list": ("Borrar lista", "Cancella elenco", "Effacer la liste", "Liste leeren", "Wyczyść listę", "Очистить список", "清除列表", "一覧をクリア", "مسح القائمة", "נקה רשימה"),
    "Open archive": ("Abrir archivo", "Apri archivio", "Ouvrir une archive", "Archiv öffnen", "Otwórz archiwum", "Открыть архив", "打开归档", "アーカイブを開く", "فتح أرشيف", "פתח ארכיון"),
    "Select an entry to preview it": (
        "Seleccione una entrada para verla", "Seleziona una voce per vederne l'anteprima", "Sélectionnez une entrée pour l'afficher", "Wählen Sie einen Eintrag für die Vorschau aus", "Wybierz pozycję, aby ją podejrzeć",
        "Выберите запись для просмотра", "选择条目以预览", "プレビューするエントリを選択してください", "اختر عنصراً لمعاينته", "בחרו פריט כדי לראות אותו בתצוגה מקדימה"),
    "Open": ("Abrir", "Apri", "Ouvrir", "Öffnen", "Otwórz", "Открыть", "打开", "開く", "فتح", "פתח"),
    "Cannot open %1:\n%2": (
        "No se puede abrir %1:\n%2", "Impossibile aprire %1:\n%2", "Impossible d'ouvrir %1 :\n%2", "%1 kann nicht geöffnet werden:\n%2", "Nie można otworzyć %1:\n%2", "Не удалось открыть %1:\n%2",
        "无法打开 %1：\n%2", "%1 を開けません：\n%2", "تعذّر فتح %1:\n%2", "לא ניתן לפתוח את %1:\n%2"),
    "Cannot open %1: %2": (
        "No se puede abrir %1: %2", "Impossibile aprire %1: %2", "Impossible d'ouvrir %1 : %2", "%1 kann nicht geöffnet werden: %2", "Nie można otworzyć %1: %2", "Не удалось открыть %1: %2",
        "无法打开 %1：%2", "%1 を開けません：%2", "تعذّر فتح %1: %2", "לא ניתן לפתוח את %1: %2"),
    "Name:    %1\n": ("Nombre:   %1\n", "Nome:     %1\n", "Nom :     %1\n", "Name:     %1\n", "Nazwa:    %1\n", "Имя:      %1\n", "名称：    %1\n", "名前：    %1\n", "الاسم:    %1\n", "שם:       %1\n"),
    "Path:    %1\n": ("Ruta:     %1\n", "Percorso: %1\n", "Chemin :  %1\n", "Pfad:     %1\n", "Ścieżka:  %1\n", "Путь:     %1\n", "路径：    %1\n", "パス：    %1\n", "المسار:   %1\n", "נתיב:     %1\n"),
    "Size:    %1 (%2 bytes)\n": (
        "Tamaño:   %1 (%2 bytes)\n", "Dimens.:  %1 (%2 byte)\n", "Taille :  %1 (%2 octets)\n", "Größe:    %1 (%2 Bytes)\n", "Rozmiar:  %1 (%2 bajtów)\n", "Размер:   %1 (%2 байт)\n",
        "大小：    %1（%2 字节）\n", "サイズ：  %1（%2 バイト）\n", "الحجم:    %1 (%2 بايت)\n", "גודל:     %1 (%2 בייטים)\n"),
    "Offset:  %1\n": ("Posición: %1\n", "Offset:   %1\n", "Décalage : %1\n", "Offset:   %1\n", "Przesun.: %1\n", "Смещение: %1\n", "偏移：    %1\n", "オフセット：%1\n", "الإزاحة:  %1\n", "היסט:     %1\n"),
    "Format:  %1\n": ("Formato:  %1\n", "Formato:  %1\n", "Format :  %1\n", "Format:   %1\n", "Format:   %1\n", "Формат:   %1\n", "格式：    %1\n", "形式：    %1\n", "الصيغة:   %1\n", "פורמט:    %1\n"),
    "File exists": ("El archivo existe", "Il file esiste", "Le fichier existe", "Datei vorhanden", "Plik istnieje", "Файл существует", "文件已存在", "ファイルが存在します", "الملف موجود", "הקובץ קיים"),
    "The file already exists:\n%1": (
        "El archivo ya existe:\n%1", "Il file esiste già:\n%1", "Le fichier existe déjà :\n%1", "Die Datei ist bereits vorhanden:\n%1", "Plik już istnieje:\n%1", "Файл уже существует:\n%1",
        "文件已存在：\n%1", "ファイルは既に存在します：\n%1", "الملف موجود بالفعل:\n%1", "הקובץ כבר קיים:\n%1"),
    "Overwrite": ("Sobrescribir", "Sovrascrivi", "Remplacer", "Überschreiben", "Zastąp", "Заменить", "覆盖", "上書き", "استبدال", "החלף"),
    "Overwrite all": ("Sobrescribir todo", "Sovrascrivi tutti", "Tout remplacer", "Alle überschreiben", "Zastąp wszystkie", "Заменить все", "全部覆盖", "すべて上書き", "استبدال الكل", "החלף הכול"),
    "Skip": ("Omitir", "Salta", "Ignorer", "Überspringen", "Pomiń", "Пропустить", "跳过", "スキップ", "تخطي", "דלג"),
    "Skip all": ("Omitir todo", "Salta tutti", "Tout ignorer", "Alle überspringen", "Pomiń wszystkie", "Пропустить все", "全部跳过", "すべてスキップ", "تخطي الكل", "דלג על הכול"),
    "Nothing is selected": ("No hay nada seleccionado", "Nessuna selezione", "Rien n'est sélectionné", "Es ist nichts ausgewählt", "Nic nie jest zaznaczone", "Ничего не выбрано", "未选择任何内容", "何も選択されていません", "لم يتم تحديد أي شيء", "לא נבחר דבר"),
    "Extract the selection to": (
        "Extraer la selección en", "Estrai la selezione in", "Extraire la sélection vers", "Auswahl extrahieren nach", "Wypakuj zaznaczenie do", "Извлечь выбранное в", "将所选项提取到", "選択項目の展開先", "استخراج المحدد إلى", "חלץ את הבחירה אל"),
    "Extract the selection (images as PNG) to": (
        "Extraer la selección (imágenes como PNG) en", "Estrai la selezione (immagini come PNG) in", "Extraire la sélection (images en PNG) vers", "Auswahl (Bilder als PNG) extrahieren nach",
        "Wypakuj zaznaczenie (obrazy jako PNG) do", "Извлечь выбранное (изображения как PNG) в", "将所选项提取到（图像保存为 PNG）", "選択項目の展開先（画像は PNG）", "استخراج المحدد (الصور بصيغة PNG) إلى", "חלץ את הבחירה (תמונות כ־PNG) אל"),
    "%1 (to .%2)": ("%1 (a .%2)", "%1 (in .%2)", "%1 (vers .%2)", "%1 (nach .%2)", "%1 (do .%2)", "%1 (в .%2)", "%1（转为 .%2）", "%1（.%2 へ）", "%1 (إلى .%2)", "%1 (אל .%2)"),
    "Extract (converted with %1) to": (
        "Extraer (convertido con %1) en", "Estrai (convertito con %1) in", "Extraire (converti avec %1) vers", "Extrahieren (konvertiert mit %1) nach", "Wypakuj (przekonwertowane za pomocą %1) do",
        "Извлечь (преобразовав с помощью %1) в", "提取到（使用 %1 转换）", "展開先（%1 で変換）", "استخراج (بعد التحويل باستخدام %1) إلى", "חלץ (מומר באמצעות %1) אל"),
    "No tool: put .ini files in %1": (
        "Ninguna herramienta: ponga archivos .ini en %1", "Nessuno strumento: inserire file .ini in %1", "Aucun outil : placez des fichiers .ini dans %1", "Kein Werkzeug: .ini-Dateien in %1 ablegen",
        "Brak narzędzi: umieść pliki .ini w %1", "Нет инструментов: поместите файлы .ini в %1", "没有工具：请将 .ini 文件放入 %1", "ツールがありません：.ini ファイルを %1 に置いてください", "لا توجد أدوات: ضع ملفات .ini في %1", "אין כלים: הניחו קובצי .ini ב־%1"),
    "Extract everything (images as PNG) to": (
        "Extraer todo (imágenes como PNG) en", "Estrai tutto (immagini come PNG) in", "Tout extraire (images en PNG) vers", "Alles (Bilder als PNG) extrahieren nach", "Wypakuj wszystko (obrazy jako PNG) do",
        "Извлечь всё (изображения как PNG) в", "全部提取到（图像保存为 PNG）", "すべての展開先（画像は PNG）", "استخراج الكل (الصور بصيغة PNG) إلى", "חלץ הכול (תמונות כ־PNG) אל"),
    "CSV (*.csv);;JSON (*.json);;Text (*.txt)": (
        "CSV (*.csv);;JSON (*.json);;Texto (*.txt)", "CSV (*.csv);;JSON (*.json);;Testo (*.txt)", "CSV (*.csv);;JSON (*.json);;Texte (*.txt)", "CSV (*.csv);;JSON (*.json);;Text (*.txt)",
        "CSV (*.csv);;JSON (*.json);;Tekst (*.txt)", "CSV (*.csv);;JSON (*.json);;Текст (*.txt)", "CSV (*.csv);;JSON (*.json);;文本 (*.txt)", "CSV (*.csv);;JSON (*.json);;テキスト (*.txt)",
        "CSV (*.csv);;JSON (*.json);;نص (*.txt)", "CSV (*.csv);;JSON (*.json);;טקסט (*.txt)"),
    "Export the list of entries": (
        "Exportar la lista de entradas", "Esporta l'elenco delle voci", "Exporter la liste des entrées", "Liste der Einträge exportieren", "Eksportuj listę pozycji", "Экспорт списка записей",
        "导出条目列表", "エントリの一覧をエクスポート", "تصدير قائمة العناصر", "ייצוא רשימת הפריטים"),
    "Export": ("Exportar", "Esporta", "Exporter", "Exportieren", "Eksportuj", "Экспорт", "导出", "エクスポート", "تصدير", "ייצא"),
    "Cannot write %1": ("No se puede escribir %1", "Impossibile scrivere %1", "Impossible d'écrire %1", "%1 kann nicht geschrieben werden", "Nie można zapisać %1", "Не удалось записать %1", "无法写入 %1", "%1 に書き込めません", "تعذّرت الكتابة إلى %1", "לא ניתן לכתוב את %1"),
    "%1 entries written to %2": (
        "%1 entradas escritas en %2", "%1 voci scritte in %2", "%1 entrées écrites dans %2", "%1 Einträge in %2 geschrieben", "Zapisano %1 pozycji do %2", "Записей в %2: %1", "已将 %1 个条目写入 %2", "%1 件のエントリを %2 に書き込みました",
        "تمت كتابة %1 عنصر في %2", "נכתבו %1 פריטים אל %2"),
    "Extract everything to": ("Extraer todo en", "Estrai tutto in", "Tout extraire vers", "Alles extrahieren nach", "Wypakuj wszystko do", "Извлечь всё в", "全部提取到", "すべての展開先", "استخراج الكل إلى", "חלץ הכול אל"),
    "Extract the folder to": ("Extraer la carpeta en", "Estrai la cartella in", "Extraire le dossier vers", "Ordner extrahieren nach", "Wypakuj folder do", "Извлечь папку в", "将文件夹提取到", "フォルダーの展開先", "استخراج المجلد إلى", "חלץ את התיקייה אל"),
    "Mesh: %1 vertices, %2 triangles": (
        "Malla: %1 vértices, %2 triángulos", "Mesh: %1 vertici, %2 triangoli", "Maillage : %1 sommets, %2 triangles", "Netz: %1 Eckpunkte, %2 Dreiecke", "Siatka: wierzchołków %1, trójkątów %2", "Меш: вершин %1, треугольников %2",
        "网格：%1 个顶点，%2 个三角形", "メッシュ：頂点 %1、三角形 %2", "شبكة: %1 رأس، %2 مثلث", "רשת: %1 קודקודים, %2 משולשים"),
    "MPEG audio options (MP3 and AAC)": (
        "Opciones de audio MPEG (MP3 y AAC)", "Opzioni audio MPEG (MP3 e AAC)", "Options audio MPEG (MP3 et AAC)", "MPEG-Audio-Optionen (MP3 und AAC)", "Opcje dźwięku MPEG (MP3 i AAC)", "Параметры звука MPEG (MP3 и AAC)",
        "MPEG 音频选项（MP3 和 AAC）", "MPEG オーディオのオプション（MP3 と AAC）", "خيارات صوت MPEG (‏MP3 وAAC)", "אפשרויות שמע MPEG (‏MP3 ו־AAC)"),
    "MP3: versions and layers to look for": (
        "MP3: versiones y capas que buscar", "MP3: versioni e livelli da cercare", "MP3 : versions et couches à rechercher", "MP3: zu suchende Versionen und Layer", "MP3: wersje i warstwy do wyszukania", "MP3: версии и слои для поиска",
        "MP3：要查找的版本和层", "MP3：検索するバージョンとレイヤー", "MP3: الإصدارات والطبقات المطلوب البحث عنها", "MP3: גרסאות ושכבות לחיפוש"),
    "Limits (MP3 and AAC)": (
        "Límites (MP3 y AAC)", "Limiti (MP3 e AAC)", "Limites (MP3 et AAC)", "Grenzen (MP3 und AAC)", "Limity (MP3 i AAC)", "Ограничения (MP3 и AAC)", "限制（MP3 和 AAC）", "制限（MP3 と AAC）", "الحدود (‏MP3 وAAC)", "מגבלות (‏MP3 ו־AAC)"),
    "At least": ("Como mínimo", "Almeno", "Au moins", "Mindestens", "Co najmniej", "Не менее", "至少", "最小", "على الأقل", "לפחות"),
    " frames": (" fotogramas", " frame", " trames", " Frames", " ramek", " кадров", " 帧", " フレーム", " إطار", " פריימים"),
    "At most (cuts longer files in pieces)": (
        "Como máximo (corta en trozos los archivos más largos)", "Al massimo (divide in parti i file più lunghi)", "Au plus (découpe les fichiers plus longs)", "Höchstens (längere Dateien werden zerteilt)",
        "Najwyżej (dłuższe pliki są dzielone na części)", "Не более (более длинные файлы разбиваются на части)", "至多（更长的文件将被切分）", "最大（これより長いファイルは分割されます）", "على الأكثر (تُقسَّم الملفات الأطول إلى أجزاء)", "לכל היותר (קבצים ארוכים יותר מפוצלים לחלקים)"),
    "At most": ("Como máximo", "Al massimo", "Au plus", "Höchstens", "Najwyżej", "Не более", "至多", "最大", "على الأكثر", "לכל היותר"),
    "Special": ("Especial", "Speciale", "Spécial", "Spezial", "Specjalne", "Особые", "特殊", "特別", "خاص", "מיוחד"),
    "Trust the size given by the Xing header of variable bit rate files": (
        "Confiar en el tamaño dado por la cabecera Xing de los archivos de tasa de bits variable", "Fidati della dimensione indicata dall'intestazione Xing dei file a bitrate variabile",
        "Se fier à la taille donnée par l'en-tête Xing des fichiers à débit variable", "Der Größe aus dem Xing-Header von Dateien mit variabler Bitrate vertrauen",
        "Ufaj rozmiarowi z nagłówka Xing plików o zmiennej szybkości bitowej", "Доверять размеру из заголовка Xing у файлов с переменным битрейтом",
        "信任可变比特率文件的 Xing 头给出的大小", "可変ビットレートファイルの Xing ヘッダーに記載されたサイズを信頼する", "الوثوق بالحجم المذكور في ترويسة Xing للملفات ذات معدل البت المتغير", "סמוך על הגודל שבכותרת Xing של קבצים בקצב סיביות משתנה"),
    "Include the ID3 tags (version 2 in front, version 1 at the end)": (
        "Incluir las etiquetas ID3 (versión 2 al principio, versión 1 al final)", "Includi i tag ID3 (versione 2 all'inizio, versione 1 alla fine)", "Inclure les balises ID3 (version 2 au début, version 1 à la fin)",
        "ID3-Tags einbeziehen (Version 2 am Anfang, Version 1 am Ende)", "Dołącz znaczniki ID3 (wersja 2 na początku, wersja 1 na końcu)", "Включать теги ID3 (версия 2 в начале, версия 1 в конце)",
        "包含 ID3 标签（版本 2 在开头，版本 1 在末尾）", "ID3 タグを含める（バージョン 2 は先頭、バージョン 1 は末尾）", "تضمين وسوم ID3 (الإصدار 2 في البداية، والإصدار 1 في النهاية)", "כלול תגיות ID3 (גרסה 2 בהתחלה, גרסה 1 בסוף)"),
    "Language of the system": (
        "Idioma del sistema", "Lingua del sistema", "Langue du système", "Systemsprache", "Język systemu", "Язык системы", "系统语言", "システムの言語", "لغة النظام", "שפת המערכת"),
    "Language (applied at the next start)": (
        "Idioma (se aplica en el próximo inicio)", "Lingua (applicata al prossimo avvio)", "Langue (appliquée au prochain démarrage)", "Sprache (wird beim nächsten Start übernommen)", "Język (zostanie zastosowany po ponownym uruchomieniu)",
        "Язык (применится при следующем запуске)", "语言（下次启动时生效）", "言語（次回の起動時に適用）", "اللغة (تُطبَّق عند التشغيل التالي)", "שפה (תחול בהפעלה הבאה)"),
    "Entries are shown as": (
        "Las entradas se muestran como", "Le voci vengono mostrate come", "Les entrées sont affichées sous forme de", "Einträge werden angezeigt als", "Pozycje są wyświetlane jako", "Записи отображаются как", "条目显示为", "エントリの表示形式", "تُعرض العناصر على شكل", "הפריטים מוצגים כ־"),
    "The folder of the .ini files that describe the command line programs used by File > Extract, converted with...": (
        "La carpeta de los archivos .ini que describen los programas de línea de comandos usados por Archivo > Extraer, convertido con...",
        "La cartella dei file .ini che descrivono i programmi da riga di comando usati da File > Estrai, convertito con...",
        "Le dossier des fichiers .ini qui décrivent les programmes en ligne de commande utilisés par Fichier > Extraire, converti avec...",
        "Der Ordner der .ini-Dateien, die die Befehlszeilenprogramme für Datei > Extrahieren, konvertiert mit ... beschreiben",
        "Folder plików .ini opisujących programy wiersza poleceń używane przez Plik > Wypakuj, przekonwertowane za pomocą...",
        "Папка с файлами .ini, описывающими программы командной строки, которые использует «Файл > Извлечь, преобразовав с помощью...»",
        "存放 .ini 文件的文件夹，这些文件描述“文件 > 提取，并使用以下工具转换...”所用的命令行程序",
        "「ファイル > 展開し、次で変換...」で使うコマンドラインプログラムを記述した .ini ファイルのフォルダー",
        "مجلد ملفات .ini التي تصف برامج سطر الأوامر المستخدمة في ملف > استخراج وتحويله باستخدام...", "תיקיית קובצי ה־.ini שמתארים את תוכניות שורת הפקודה שבהן משתמשת הפקודה קובץ > חלץ, מומרים באמצעות..."),
    "Folder of the conversion tools": (
        "Carpeta de las herramientas de conversión", "Cartella degli strumenti di conversione", "Dossier des outils de conversion", "Ordner der Konvertierungswerkzeuge", "Folder narzędzi do konwersji", "Папка инструментов преобразования",
        "转换工具所在文件夹", "変換ツールのフォルダー", "مجلد أدوات التحويل", "תיקיית כלי ההמרה"),
    "General": ("General", "Generale", "Général", "Allgemein", "Ogólne", "Общие", "常规", "全般", "عام", "כללי"),
    "Largest image to preview": (
        "Imagen más grande que previsualizar", "Immagine più grande da visualizzare in anteprima", "Plus grande image à afficher en aperçu", "Größtes Bild für die Vorschau", "Największy obraz do podglądu", "Наибольшее изображение для просмотра",
        "预览的最大图像", "プレビューする最大の画像", "أكبر صورة للمعاينة", "התמונה הגדולה ביותר לתצוגה מקדימה"),
    "Largest text to preview": (
        "Texto más grande que previsualizar", "Testo più grande da visualizzare in anteprima", "Plus grand texte à afficher en aperçu", "Größter Text für die Vorschau", "Największy tekst do podglądu", "Наибольший текст для просмотра",
        "预览的最大文本", "プレビューする最大のテキスト", "أكبر نص للمعاينة", "הטקסט הגדול ביותר לתצוגה מקדימה"),
    "Used for the Quake pictures and the Build tiles when the archive has no PALETTE.DAT or PALETTE.LMP (768 bytes, or a JASC .pal file)": (
        "Se usa para las imágenes de Quake y los mosaicos de Build cuando el archivo no tiene PALETTE.DAT ni PALETTE.LMP (768 bytes, o un archivo .pal JASC)",
        "Usata per le immagini di Quake e i tile di Build quando l'archivio non ha PALETTE.DAT né PALETTE.LMP (768 byte, o un file .pal JASC)",
        "Utilisée pour les images de Quake et les tuiles de Build lorsque l'archive n'a ni PALETTE.DAT ni PALETTE.LMP (768 octets, ou un fichier .pal JASC)",
        "Wird für die Quake-Bilder und Build-Kacheln verwendet, wenn das Archiv weder PALETTE.DAT noch PALETTE.LMP enthält (768 Bytes oder eine JASC-.pal-Datei)",
        "Używana dla obrazów Quake i kafelków Build, gdy archiwum nie zawiera PALETTE.DAT ani PALETTE.LMP (768 bajtów lub plik .pal JASC)",
        "Используется для изображений Quake и плиток Build, если в архиве нет PALETTE.DAT или PALETTE.LMP (768 байт или файл .pal JASC)",
        "当归档中没有 PALETTE.DAT 或 PALETTE.LMP 时，用于 Quake 图片和 Build 图块（768 字节，或 JASC .pal 文件）",
        "アーカイブに PALETTE.DAT も PALETTE.LMP もない場合に、Quake の画像と Build のタイルに使用します（768 バイト、または JASC の .pal ファイル）",
        "يُستخدم لصور Quake ولبنات Build عندما لا يحتوي الأرشيف على PALETTE.DAT أو PALETTE.LMP (768 بايت، أو ملف .pal بصيغة JASC)",
        "משמש לתמונות Quake ולאריחי Build כאשר בארכיון אין PALETTE.DAT או PALETTE.LMP (768 בייטים, או קובץ .pal של JASC)"),
    "Palette": ("Paleta", "Tavolozza", "Palette", "Palette", "Paleta", "Палитра", "调色板", "パレット", "لوحة الألوان", "פלטת צבעים"),
    "Palettes (*.pal *.lmp *.dat *.bin *.raw);;All files (*)": (
        "Paletas (*.pal *.lmp *.dat *.bin *.raw);;Todos los archivos (*)", "Tavolozze (*.pal *.lmp *.dat *.bin *.raw);;Tutti i file (*)", "Palettes (*.pal *.lmp *.dat *.bin *.raw);;Tous les fichiers (*)",
        "Paletten (*.pal *.lmp *.dat *.bin *.raw);;Alle Dateien (*)", "Palety (*.pal *.lmp *.dat *.bin *.raw);;Wszystkie pliki (*)", "Палитры (*.pal *.lmp *.dat *.bin *.raw);;Все файлы (*)",
        "调色板 (*.pal *.lmp *.dat *.bin *.raw);;所有文件 (*)", "パレット (*.pal *.lmp *.dat *.bin *.raw);;すべてのファイル (*)", "لوحات الألوان (*.pal *.lmp *.dat *.bin *.raw);;كل الملفات (*)", "פלטות (*.pal *.lmp *.dat *.bin *.raw);;כל הקבצים (*)"),
    "Palette of the 8 bits pictures": (
        "Paleta de las imágenes de 8 bits", "Tavolozza delle immagini a 8 bit", "Palette des images 8 bits", "Palette der 8-Bit-Bilder", "Paleta obrazów 8-bitowych", "Палитра 8-битных изображений",
        "8 位图片的调色板", "8 ビット画像のパレット", "لوحة ألوان الصور ذات 8 بتات", "פלטת התמונות של 8 סיביות"),
    "Preview": ("Vista previa", "Anteprima", "Aperçu", "Vorschau", "Podgląd", "Просмотр", "预览", "プレビュー", "المعاينة", "תצוגה מקדימה"),
    "Messages shown in the Console": (
        "Mensajes mostrados en la consola", "Messaggi mostrati nella console", "Messages affichés dans la console", "In der Konsole angezeigte Meldungen", "Komunikaty wyświetlane w konsoli", "Сообщения, показываемые в консоли",
        "控制台中显示的消息", "コンソールに表示するメッセージ", "الرسائل المعروضة في وحدة التحكم", "הודעות המוצגות במסוף"),
    "Log": ("Registro", "Registro", "Journal", "Protokoll", "Dziennik", "Журнал", "日志", "ログ", "السجل", "יומן"),
    "%1 (%2 formats)\n%3": ("%1 (%2 formatos)\n%3", "%1 (%2 formati)\n%3", "%1 (%2 formats)\n%3", "%1 (%2 Formate)\n%3", "%1 (formatów: %2)\n%3", "%1 (форматов: %2)\n%3", "%1（%2 种格式）\n%3", "%1（%2 形式）\n%3", "%1 (%2 صيغة)\n%3", "%1 (%2 פורמטים)\n%3"),
    "Extensions": ("Extensiones", "Estensioni", "Extensions", "Erweiterungen", "Rozszerzenia", "Расширения", "扩展名", "拡張子", "الامتدادات", "סיומות"),
    "The driver is not loaded: %1": (
        "El controlador no está cargado: %1", "Il driver non è caricato: %1", "Le pilote n'est pas chargé : %1", "Der Treiber ist nicht geladen: %1", "Sterownik nie jest załadowany: %1", "Драйвер не загружен: %1",
        "驱动程序未加载：%1", "ドライバーが読み込まれていません：%1", "برنامج التشغيل غير محمَّل: %1", "המנהל אינו טעון: %1"),
    "Play": ("Reproducir", "Riproduci", "Lire", "Wiedergabe", "Odtwórz", "Воспроизвести", "播放", "再生", "تشغيل", "נגן"),
    "Pause": ("Pausa", "Pausa", "Pause", "Pause", "Pauza", "Пауза", "暂停", "一時停止", "إيقاف مؤقت", "השהה"),
    "%1 / %2": ("%1 / %2",) * 10,
    "Cannot play this sound: %1": (
        "No se puede reproducir este sonido: %1", "Impossibile riprodurre questo suono: %1", "Impossible de lire ce son : %1", "Dieser Ton kann nicht wiedergegeben werden: %1", "Nie można odtworzyć tego dźwięku: %1",
        "Не удалось воспроизвести этот звук: %1", "无法播放此声音：%1", "このサウンドを再生できません：%1", "تعذّر تشغيل هذا الصوت: %1", "לא ניתן לנגן את הצליל הזה: %1"),
    "Sound (%1, %2)": ("Sonido (%1, %2)", "Suono (%1, %2)", "Son (%1, %2)", "Ton (%1, %2)", "Dźwięk (%1, %2)", "Звук (%1, %2)", "声音（%1，%2）", "サウンド（%1、%2）", "صوت (%1، %2)", "צליל (%1, %2)"),
    "Image %1 x %2": ("Imagen %1 x %2", "Immagine %1 x %2", "Image %1 x %2", "Bild %1 x %2", "Obraz %1 x %2", "Изображение %1 × %2", "图像 %1 × %2", "画像 %1 × %2", "صورة %1 × %2", "תמונה %1 × %2"),
    "Cannot decode the image": (
        "No se puede decodificar la imagen", "Impossibile decodificare l'immagine", "Impossible de décoder l'image", "Das Bild kann nicht dekodiert werden", "Nie można zdekodować obrazu", "Не удалось декодировать изображение",
        "无法解码图像", "画像をデコードできません", "تعذّر فك ترميز الصورة", "לא ניתן לפענח את התמונה"),
    "Text": ("Texto", "Testo", "Texte", "Text", "Tekst", "Текст", "文本", "テキスト", "نص", "טקסט"),
    "Hex dump": ("Volcado hexadecimal", "Dump esadecimale", "Vidage hexadécimal", "Hexdump", "Zrzut szesnastkowy", "Шестнадцатеричный дамп", "十六进制转储", "16 進ダンプ", "تفريغ سداسي عشري", "תוכן הקסדצימלי"),
    "Sound": ("Sonido", "Suono", "Son", "Ton", "Dźwięk", "Звук", "声音", "サウンド", "صوت", "צליל"),
    "Cannot preview: %1": ("No se puede previsualizar: %1", "Impossibile mostrare l'anteprima: %1", "Impossible d'afficher l'aperçu : %1", "Vorschau nicht möglich: %1", "Nie można wyświetlić podglądu: %1", "Не удалось показать просмотр: %1",
                           "无法预览：%1", "プレビューできません：%1", "تعذّرت المعاينة: %1", "לא ניתן להציג תצוגה מקדימה: %1"),
    "No preview": ("Sin vista previa", "Nessuna anteprima", "Pas d'aperçu", "Keine Vorschau", "Brak podglądu", "Нет просмотра", "无预览", "プレビューなし", "لا توجد معاينة", "אין תצוגה מקדימה"),
    "No preview (%1)": ("Sin vista previa (%1)", "Nessuna anteprima (%1)", "Pas d'aperçu (%1)", "Keine Vorschau (%1)", "Brak podglądu (%1)", "Нет просмотра (%1)", "无预览（%1）", "プレビューなし（%1）", "لا توجد معاينة (%1)", "אין תצוגה מקדימה (%1)"),
}

PT_BR = {
    "Looks for files embedded in any file by their signatures (images, sounds, archives...).": "Procura arquivos embutidos em qualquer arquivo pelas suas assinaturas (imagens, sons, pacotes...).",
    "Looks for files embedded in any file by their signatures (images, sounds, videos, archives...).": "Procura arquivos embutidos em qualquer arquivo pelas suas assinaturas (imagens, sons, vídeos, pacotes...).",
    "Leave out the formats that give many false positives": "Omitir os formatos que dão muitos falsos positivos",
    "MP3 / AAC options...": "Opções de MP3 / AAC...",
    "Start the scan as soon as this window opens on a file": "Iniciar o exame assim que esta janela abrir com um arquivo",
    "Open the files found as soon as the scan ends": "Abrir os arquivos encontrados assim que o exame terminar",
    "Name the files with this template:": "Nomear os arquivos com este modelo:",
    "Put the files in the folders Audio, Video, Image and Unknown": "Colocar os arquivos nas pastas Áudio, Vídeo, Imagem e Desconhecido",
    "Save an HRF file (to open the finds again without scanning)": "Salvar um arquivo HRF (para reabrir os achados sem examinar de novo)",
    "Add information:": "Adicionar informações:",
    "Do not store the program and its version (version 3)": "Não gravar o programa e sua versão (versão 3)",
    "Browse...": "Procurar...",
    "All": "Todos", "Images": "Imagens", "Videos": "Vídeos", "Audio": "Áudio", "Formats": "Formatos",
    "%f name of the scanned file, %x its extension, %o offset, %h offset in hexadecimal, %s size, %l size in hexadecimal, %n number of the find. The default is %f_%x-%n.":
        "%f nome do arquivo examinado, %x sua extensão, %o posição, %h posição em hexadecimal, %s tamanho, %l tamanho em hexadecimal, %n número do achado. O padrão é %f_%x-%n.",
    "Names": "Nomes", "File": "Arquivo",
    "Version 3.0 (64 bits offsets, long names)": "Versão 3.0 (posições de 64 bits, nomes longos)",
    "Version 2 (names of 63 characters)": "Versão 2 (nomes de 63 caracteres)",
    "Version 1 (names of 31 characters)": "Versão 1 (nomes de 31 caracteres)",
    "Format": "Formato", "Title": "Título", "Author": "Autor", "URL": "URL", "HRF file": "Arquivo HRF", " KiB": " KiB",
    "What is read from the file at once: only the speed changes, not the finds": "O que é lido do arquivo de uma vez: só a velocidade muda, não os achados",
    "Read buffer": "Buffer de leitura", "Options": "Opções",
    "HyperRipper files (*.hrf)": "Arquivos HyperRipper (*.hrf)",
    "Speed %1/s    Scanned %2    Left %3    Elapsed %4    Remaining %5": "Velocidade %1/s    Examinado %2    Restante %3    Decorrido %4    Faltam %5",
    ", saved in %1": ", salvo em %1",
    ", but the HRF file was not saved: %1": ", mas o arquivo HRF não foi salvo: %1",
    "Nothing found": "Nada encontrado",
    "Extract selected, converted with...": "Extrair selecionados, convertidos com...",
    "Extract all, converted with...": "Extrair tudo, convertido com...",
    "Where is %1?": "Onde está %1?",
    "%1 is not the file the HRF file was made for (%2 bytes instead of %3)": "%1 não é o arquivo para o qual o arquivo HRF foi feito (%2 bytes em vez de %3)",
    "The HRF file is damaged: %1 is outside of the file": "O arquivo HRF está danificado: %1 está fora do arquivo",
    "%1 (to .%2)": "%1 (para .%2)",
    "Extract (converted with %1) to": "Extrair (convertido com %1) para",
    "No tool: put .ini files in %1": "Nenhuma ferramenta: coloque arquivos .ini em %1",
    "MPEG audio options (MP3 and AAC)": "Opções de áudio MPEG (MP3 e AAC)",
    "MP3: versions and layers to look for": "MP3: versões e camadas a procurar",
    "Limits (MP3 and AAC)": "Limites (MP3 e AAC)",
    "At least": "No mínimo", " frames": " quadros", " bytes": " bytes",
    "At most (cuts longer files in pieces)": "No máximo (corta arquivos maiores em partes)",
    "At most": "No máximo", "Special": "Especial",
    "Trust the size given by the Xing header of variable bit rate files": "Confiar no tamanho dado pelo cabeçalho Xing dos arquivos de taxa de bits variável",
    "Include the ID3 tags (version 2 in front, version 1 at the end)": "Incluir as etiquetas ID3 (versão 2 na frente, versão 1 no fim)",
    "The folder of the .ini files that describe the command line programs used by File > Extract, converted with...": "A pasta dos arquivos .ini que descrevem os programas de linha de comando usados por Arquivo > Extrair, convertido com...",
    "Folder of the conversion tools": "Pasta das ferramentas de conversão",
    "Used for the Quake pictures and the Build tiles when the archive has no PALETTE.DAT or PALETTE.LMP (768 bytes, or a JASC .pal file)":
        "Usada nas imagens do Quake e nos blocos do Build quando o pacote não tem PALETTE.DAT nem PALETTE.LMP (768 bytes, ou um arquivo .pal JASC)",
    "Palette": "Paleta",
    "Palettes (*.pal *.lmp *.dat *.bin *.raw);;All files (*)": "Paletas (*.pal *.lmp *.dat *.bin *.raw);;Todos os arquivos (*)",
    "Palette of the 8 bits pictures": "Paleta das imagens de 8 bits",
}


# The lists of entries made from templates (File > Create a list from a template)
TRANSLATIONS.update({
    "Every entry": ("Todas las entradas", "Tutte le voci", "Toutes les entrées", "Alle Einträge", "Wszystkie pozycje", "Все записи", "所有条目", "すべてのエントリ", "كل العناصر", "כל הפריטים"),
    "The current folder": ("La carpeta actual", "La cartella corrente", "Le dossier courant", "Der aktuelle Ordner", "Bieżący folder", "Текущая папка", "当前文件夹", "現在のフォルダー", "المجلد الحالي", "התיקייה הנוכחית"),
    "The selected entries (%1)": (
        "Las entradas seleccionadas (%1)", "Le voci selezionate (%1)", "Les entrées sélectionnées (%1)", "Die ausgewählten Einträge (%1)", "Zaznaczone pozycje (%1)", "Выбранные записи (%1)",
        "所选条目（%1）", "選択したエントリ（%1）", "العناصر المحددة (%1)", "הפריטים שנבחרו (%1)"),
    "Include the subfolders": ("Incluir las subcarpetas", "Includi le sottocartelle", "Inclure les sous-dossiers", "Unterordner einbeziehen", "Uwzględnij podfoldery", "Включая вложенные папки", "包含子文件夹", "サブフォルダーを含める", "تضمين المجلدات الفرعية", "כלול תיקיות משנה"),
    "Sort the list": ("Ordenar la lista", "Ordina l'elenco", "Trier la liste", "Liste sortieren", "Sortuj listę", "Сортировать список", "对列表排序", "一覧を並べ替える", "ترتيب القائمة", "מיין את הרשימה"),
    "by name": ("por nombre", "per nome", "par nom", "nach Name", "według nazwy", "по имени", "按名称", "名前順", "حسب الاسم", "לפי שם"),
    "by size": ("por tamaño", "per dimensione", "par taille", "nach Größe", "według rozmiaru", "по размеру", "按大小", "サイズ順", "حسب الحجم", "לפי גודל"),
    "by offset": ("por desplazamiento", "per offset", "par décalage", "nach Offset", "według przesunięcia", "по смещению", "按偏移", "オフセット順", "حسب الإزاحة", "לפי היסט"),
    "Invert the order": ("Invertir el orden", "Inverti l'ordine", "Inverser l'ordre", "Reihenfolge umkehren", "Odwróć kolejność", "Обратный порядок", "反转顺序", "順序を逆にする", "عكس الترتيب", "הפוך את הסדר"),
    "Create a list from a template": (
        "Crear una lista a partir de una plantilla", "Crea un elenco da un modello", "Créer une liste à partir d'un modèle", "Liste aus einer Vorlage erstellen", "Utwórz listę z szablonu", "Создать список по шаблону",
        "根据模板创建列表", "テンプレートから一覧を作成", "إنشاء قائمة من قالب", "צור רשימה מתבנית"),
    "Entries": ("Entradas", "Voci", "Entrées", "Einträge", "Pozycje", "Записи", "条目", "エントリ", "العناصر", "פריטים"),
    "Order": ("Orden", "Ordine", "Ordre", "Reihenfolge", "Kolejność", "Порядок", "顺序", "順序", "الترتيب", "סדר"),
    "Create...": ("Crear...", "Crea...", "Créer...", "Erstellen ...", "Utwórz...", "Создать...", "创建...", "作成...", "إنشاء...", "צור..."),
    "Version: %1\nAuthor: %2": (
        "Versión: %1\nAutor: %2", "Versione: %1\nAutore: %2", "Version : %1\nAuteur : %2", "Version: %1\nAutor: %2", "Wersja: %1\nAutor: %2", "Версия: %1\nАвтор: %2", "版本：%1\n作者：%2", "バージョン：%1\n作成者：%2",
        "الإصدار: %1\nالمؤلف: %2", "גרסה: %1\nמחבר: %2"),
    "Email: %1": ("Correo electrónico: %1", "E-mail: %1", "Courriel : %1", "E-Mail: %1", "E-mail: %1", "Эл. почта: %1", "电子邮件：%1", "メール：%1", "البريد الإلكتروني: %1", "דוא״ל: %1"),
    "URL: %1": ("URL: %1",) * 10,
    "Create a list from a template...": (
        "Crear una lista a partir de una plantilla...", "Crea un elenco da un modello...", "Créer une liste à partir d'un modèle...", "Liste aus einer Vorlage erstellen ...", "Utwórz listę z szablonu...", "Создать список по шаблону...",
        "根据模板创建列表...", "テンプレートから一覧を作成...", "إنشاء قائمة من قالب...", "צור רשימה מתבנית..."),
    "Save the list to...": ("Guardar la lista en...", "Salva l'elenco in...", "Enregistrer la liste dans...", "Liste speichern unter ...", "Zapisz listę w...", "Сохранить список в...", "将列表保存到...", "一覧の保存先...", "حفظ القائمة في...", "שמור את הרשימה ב־..."),
    "The templates of your own for File > Create a list from a template (.uht files, or folders with a .tsf source)": (
        "Sus propias plantillas para Archivo > Crear una lista a partir de una plantilla (archivos .uht, o carpetas con un origen .tsf)",
        "I modelli personali per File > Crea un elenco da un modello (file .uht, o cartelle con un sorgente .tsf)",
        "Vos propres modèles pour Fichier > Créer une liste à partir d'un modèle (fichiers .uht, ou dossiers avec une source .tsf)",
        "Eigene Vorlagen für Datei > Liste aus einer Vorlage erstellen (.uht-Dateien oder Ordner mit einer .tsf-Quelle)",
        "Własne szablony dla Plik > Utwórz listę z szablonu (pliki .uht lub foldery ze źródłem .tsf)",
        "Ваши собственные шаблоны для «Файл > Создать список по шаблону» (файлы .uht или папки с исходником .tsf)",
        "供“文件 > 根据模板创建列表”使用的自定义模板（.uht 文件，或包含 .tsf 源的文件夹）",
        "「ファイル > テンプレートから一覧を作成」で使う独自のテンプレート（.uht ファイル、または .tsf ソースを含むフォルダー）",
        "قوالبك الخاصة لـ ملف > إنشاء قائمة من قالب (ملفات .uht، أو مجلدات تحتوي على مصدر .tsf)", "התבניות שלכם עבור קובץ > צור רשימה מתבנית (קובצי .uht, או תיקיות עם מקור .tsf)"),
    "Folder of the list templates": (
        "Carpeta de las plantillas de listas", "Cartella dei modelli di elenco", "Dossier des modèles de liste", "Ordner der Listenvorlagen", "Folder szablonów list", "Папка шаблонов списков", "列表模板所在文件夹", "一覧テンプレートのフォルダー", "مجلد قوالب القوائم", "תיקיית תבניות הרשימות"),
    "Size (Bytes)": ("Tamaño (bytes)", "Dimensione (byte)", "Taille (octets)", "Größe (Bytes)", "Rozmiar (bajty)", "Размер (байты)", "大小（字节）", "サイズ（バイト）", "الحجم (بايت)", "גודל (בייטים)"),
    "Description": ("Descripción", "Descrizione", "Description", "Beschreibung", "Opis", "Описание", "说明", "説明", "الوصف", "תיאור"),
    "Data X": ("Datos X", "Dati X", "Données X", "Daten X", "Dane X", "Данные X", "数据 X", "データ X", "بيانات X", "נתונים X"),
    "Data Y": ("Datos Y", "Dati Y", "Données Y", "Daten Y", "Dane Y", "Данные Y", "数据 Y", "データ Y", "بيانات Y", "נתונים Y"),
    "File %e": ("Archivo %e", "File %e", "Fichier %e", "Datei %e", "Plik %e", "Файл %e", "%e 文件", "%e ファイル", "ملف %e", "קובץ %e"),
    "Animation (%e)": ("Animación (%e)", "Animazione (%e)", "Animation (%e)", "Animation (%e)", "Animacja (%e)", "Анимация (%e)", "动画（%e）", "アニメーション（%e）", "رسوم متحركة (%e)", "הנפשה (%e)"),
    "Art Package (Textures)": ("Paquete de arte (texturas)", "Pacchetto grafico (texture)", "Paquet graphique (textures)", "Grafikpaket (Texturen)", "Pakiet grafiki (tekstury)", "Пакет графики (текстуры)", "美术资源包（纹理）", "アートパッケージ（テクスチャ）", "حزمة فنية (خامات)", "חבילת אמנות (מרקמים)"),
    "Data (%e)": ("Datos (%e)", "Dati (%e)", "Données (%e)", "Daten (%e)", "Dane (%e)", "Данные (%e)", "数据（%e）", "データ（%e）", "بيانات (%e)", "נתונים (%e)"),
    "Configuration (%e)": ("Configuración (%e)", "Configurazione (%e)", "Configuration (%e)", "Konfiguration (%e)", "Konfiguracja (%e)", "Конфигурация (%e)", "配置（%e）", "設定（%e）", "إعدادات (%e)", "תצורה (%e)"),
    "Dynamic Link Library": ("Biblioteca de vínculos dinámicos", "Libreria a collegamento dinamico", "Bibliothèque de liens dynamiques", "Dynamische Bibliothek", "Biblioteka dołączana dynamicznie", "Динамически подключаемая библиотека", "动态链接库", "ダイナミックリンクライブラリ", "مكتبة الارتباط الديناميكي", "ספריית קישור דינמית"),
    "Fire Texture (Dynamic)": ("Textura de fuego (dinámica)", "Texture di fuoco (dinamica)", "Texture de feu (dynamique)", "Feuertextur (dynamisch)", "Tekstura ognia (dynamiczna)", "Текстура огня (динамическая)", "火焰纹理（动态）", "ファイアテクスチャ（動的）", "خامة نار (ديناميكية)", "מרקם אש (דינמי)"),
    "Image %e (%d)": ("Imagen %e (%d)", "Immagine %e (%d)", "Image %e (%d)", "Bild %e (%d)", "Obraz %e (%d)", "Изображение %e (%d)", "图像 %e（%d）", "画像 %e（%d）", "صورة %e (%d)", "תמונה %e (%d)"),
    "Level Map (%e)": ("Mapa de nivel (%e)", "Mappa del livello (%e)", "Carte de niveau (%e)", "Levelkarte (%e)", "Mapa poziomu (%e)", "Карта уровня (%e)", "关卡地图（%e）", "レベルマップ（%e）", "خريطة مستوى (%e)", "מפת שלב (%e)"),
    "3D Model (%e)": ("Modelo 3D (%e)", "Modello 3D (%e)", "Modèle 3D (%e)", "3D-Modell (%e)", "Model 3D (%e)", "3D-модель (%e)", "3D 模型（%e）", "3D モデル（%e）", "نموذج ثلاثي الأبعاد (%e)", "מודל תלת־ממדי (%e)"),
    "MPEG Audio/Video": ("Audio/vídeo MPEG", "Audio/video MPEG", "Audio/vidéo MPEG", "MPEG-Audio/-Video", "Dźwięk/wideo MPEG", "Аудио/видео MPEG", "MPEG 音频/视频", "MPEG オーディオ/ビデオ", "صوت/فيديو MPEG", "שמע/וידאו MPEG"),
    "Music (%d)": ("Música (%d)", "Musica (%d)", "Musique (%d)", "Musik (%d)", "Muzyka (%d)", "Музыка (%d)", "音乐（%d）", "音楽（%d）", "موسيقى (%d)", "מוזיקה (%d)"),
    "Script (%e)": ("Script (%e)", "Script (%e)", "Script (%e)", "Skript (%e)", "Skrypt (%e)", "Скрипт (%e)", "脚本（%e）", "スクリプト（%e）", "سكربت (%e)", "סקריפט (%e)"),
    "Sound (%d)": ("Sonido (%d)", "Suono (%d)", "Son (%d)", "Ton (%d)", "Dźwięk (%d)", "Звук (%d)", "声音（%d）", "サウンド（%d）", "صوت (%d)", "צליל (%d)"),
    "Sprite": ("Sprite", "Sprite", "Sprite", "Sprite", "Sprite", "Спрайт", "精灵", "スプライト", "سبرايت", "ספרייט"),
    "Texture (%e)": ("Textura (%e)", "Texture (%e)", "Texture (%e)", "Textur (%e)", "Tekstura (%e)", "Текстура (%e)", "纹理（%e）", "テクスチャ（%e）", "خامة (%e)", "מרקם (%e)"),
    "Text Document": ("Documento de texto", "Documento di testo", "Document texte", "Textdokument", "Dokument tekstowy", "Текстовый документ", "文本文档", "テキスト文書", "مستند نصي", "מסמך טקסט"),
})

PT_BR.update({
    "Every entry": "Todas as entradas", "The current folder": "A pasta atual", "The selected entries (%1)": "As entradas selecionadas (%1)",
    "Include the subfolders": "Incluir as subpastas", "Sort the list": "Ordenar a lista", "by name": "por nome", "by size": "por tamanho", "by offset": "por posição",
    "Invert the order": "Inverter a ordem", "Create a list from a template": "Criar uma lista a partir de um modelo", "Entries": "Entradas", "Order": "Ordem", "Create...": "Criar...",
    "Version: %1\nAuthor: %2": "Versão: %1\nAutor: %2", "Email: %1": "E-mail: %1", "URL: %1": "URL: %1",
    "Create a list from a template...": "Criar uma lista a partir de um modelo...", "Save the list to...": "Salvar a lista em...",
    "The templates of your own for File > Create a list from a template (.uht files, or folders with a .tsf source)": "Os seus próprios modelos para Arquivo > Criar uma lista a partir de um modelo (arquivos .uht, ou pastas com uma fonte .tsf)",
    "Folder of the list templates": "Pasta dos modelos de lista",
    "Size (Bytes)": "Tamanho (bytes)", "Description": "Descrição", "Data X": "Dados X", "Data Y": "Dados Y", "File %e": "Arquivo %e", "Animation (%e)": "Animação (%e)",
    "Art Package (Textures)": "Pacote de arte (texturas)", "Data (%e)": "Dados (%e)", "Configuration (%e)": "Configuração (%e)", "Dynamic Link Library": "Biblioteca de vínculo dinâmico",
    "Fire Texture (Dynamic)": "Textura de fogo (dinâmica)", "Image %e (%d)": "Imagem %e (%d)", "Level Map (%e)": "Mapa de fase (%e)", "3D Model (%e)": "Modelo 3D (%e)",
    "MPEG Audio/Video": "Áudio/vídeo MPEG", "Music (%d)": "Música (%d)", "Script (%e)": "Script (%e)", "Sound (%d)": "Som (%d)", "Sprite": "Sprite", "Texture (%e)": "Textura (%e)",
    "Text Document": "Documento de texto", "Offset": "Deslocamento",
})


# The identification of the file types (Tools > Identify the file types)
TRANSLATIONS.update({
    "Detected type": ("Tipo detectado", "Tipo rilevato", "Type détecté", "Erkannter Typ", "Wykryty typ", "Определённый тип", "检测到的类型", "検出した種類", "النوع المكتشف", "סוג שזוהה"),
    "Detected: %1 (%2%)": (
        "Detectado: %1 (%2%)", "Rilevato: %1 (%2%)", "Détecté : %1 (%2 %)", "Erkannt: %1 (%2 %)", "Wykryto: %1 (%2%)", "Определено: %1 (%2%)", "检测到：%1（%2%）", "検出：%1（%2%）", "تم اكتشاف: %1 (%2%)", "זוהה: %1 (%2%)"),
    "Detected: %1": ("Detectado: %1", "Rilevato: %1", "Détecté : %1", "Erkannt: %1", "Wykryto: %1", "Определено: %1", "检测到：%1", "検出：%1", "تم اكتشاف: %1", "זוהה: %1"),
    "Identify the file types": (
        "Identificar los tipos de archivo", "Identifica i tipi di file", "Identifier les types de fichier", "Dateitypen erkennen", "Rozpoznaj typy plików", "Определить типы файлов", "识别文件类型", "ファイルの種類を判別", "التعرف على أنواع الملفات", "זהה את סוגי הקבצים"),
    "Identifying the file types: %1 of %2": (
        "Identificando los tipos de archivo: %1 de %2", "Identificazione dei tipi di file: %1 di %2", "Identification des types de fichier : %1 sur %2", "Dateitypen werden erkannt: %1 von %2", "Rozpoznawanie typów plików: %1 z %2",
        "Определение типов файлов: %1 из %2", "正在识别文件类型：%1 / %2", "ファイルの種類を判別中：%1 / %2", "جارٍ التعرف على أنواع الملفات: %1 من %2", "מזהה את סוגי הקבצים: %1 מתוך %2"),
    "%1 file types identified: %2": (
        "%1 tipos de archivo identificados: %2", "%1 tipi di file identificati: %2", "%1 types de fichier identifiés : %2", "%1 Dateitypen erkannt: %2", "Rozpoznano typów plików: %1: %2", "Определено типов файлов: %1: %2",
        "已识别 %1 个文件类型：%2", "%1 件のファイルの種類を判別しました：%2", "تم التعرف على %1 من أنواع الملفات: %2", "זוהו %1 סוגי קבצים: %2"),
    "When extracting, give the files without an extension the one their content says": (
        "Al extraer, dar a los archivos sin extensión la que indica su contenido", "Durante l'estrazione, dai ai file senza estensione quella indicata dal contenuto",
        "À l'extraction, donner aux fichiers sans extension celle que leur contenu indique", "Beim Extrahieren Dateien ohne Erweiterung die Erweiterung geben, die ihr Inhalt angibt",
        "Podczas wypakowywania nadawaj plikom bez rozszerzenia to, które wynika z ich zawartości", "При извлечении давать файлам без расширения то, которое определяет их содержимое",
        "提取时，为没有扩展名的文件加上由内容判断出的扩展名", "展開時、拡張子のないファイルに内容から判別した拡張子を付ける", "عند الاستخراج، امنح الملفات التي لا امتداد لها الامتداد الذي يدل عليه محتواها", "בעת חילוץ, תן לקבצים ללא סיומת את הסיומת שתוכנם מעיד עליה"),
    "For the archives that name their files with numbers or hashes: a PNG image becomes name.png (only when the type is certain)": (
        "Para los archivos que nombran sus ficheros con números o hashes: una imagen PNG pasa a ser nombre.png (solo cuando el tipo es seguro)",
        "Per gli archivi che chiamano i file con numeri o hash: un'immagine PNG diventa nome.png (solo quando il tipo è certo)",
        "Pour les archives qui nomment leurs fichiers par des numéros ou des empreintes : une image PNG devient nom.png (seulement quand le type est certain)",
        "Für Archive, die ihre Dateien mit Nummern oder Hashes benennen: ein PNG-Bild wird zu name.png (nur wenn der Typ sicher ist)",
        "Dla archiwów nazywających pliki numerami lub skrótami: obraz PNG staje się nazwa.png (tylko gdy typ jest pewny)",
        "Для архивов, называющих файлы числами или хешами: изображение PNG получит имя имя.png (только если тип достоверен)",
        "适用于以数字或哈希值命名文件的归档：PNG 图像将变为 名称.png（仅在类型确定时）",
        "ファイルに番号やハッシュを名前として使うアーカイブ向け：PNG 画像は 名前.png になります（種類が確実な場合のみ）",
        "للأرشيفات التي تسمّي ملفاتها بأرقام أو بصمات: تصبح صورة PNG باسم الاسم.png (فقط عندما يكون النوع مؤكداً)", "לארכיונים שנותנים לקבצים שמות של מספרים או גיבובים: תמונת PNG הופכת ל־שם.png (רק כשהסוג בטוח)"),
})

PT_BR.update({
    "Detected type": "Tipo detectado", "Detected: %1 (%2%)": "Detectado: %1 (%2%)", "Detected: %1": "Detectado: %1", "Identify the file types": "Identificar os tipos de arquivo",
    "Identifying the file types: %1 of %2": "Identificando os tipos de arquivo: %1 de %2", "%1 file types identified: %2": "%1 tipos de arquivo identificados: %2",
    "When extracting, give the files without an extension the one their content says": "Ao extrair, dar aos arquivos sem extensão a que o conteúdo indica",
    "For the archives that name their files with numbers or hashes: a PNG image becomes name.png (only when the type is certain)": "Para os pacotes que nomeiam os arquivos com números ou hashes: uma imagem PNG vira nome.png (só quando o tipo é certo)",
})


# The About dialog
TRANSLATIONS.update({
    "Explore, preview and extract the resources of games": ("Explora, previsualiza y extrae los recursos de los juegos", "Esplora, visualizza ed estrai le risorse dei giochi", "Explorez, prévisualisez et extrayez les ressources des jeux", "Ressourcen von Spielen durchsuchen, ansehen und extrahieren", "Przeglądaj, podglądaj i wypakowuj zasoby gier", "Просмотр, предпросмотр и извлечение ресурсов игр", "浏览、预览并提取游戏资源", "ゲームのリソースを閲覧・プレビュー・展開", "استكشف موارد الألعاب وعاينها واستخرجها", "עיון, תצוגה מקדימה וחילוץ של משאבי משחקים"),
    "Version %1": ("Versión %1", "Versione %1", "Version %1", "Version %1", "Wersja %1", "Версия %1", "版本 %1", "バージョン %1", "الإصدار %1", "גרסה %1"),
    "Credits": ("Créditos", "Crediti", "Crédits", "Danksagung", "Autorzy", "Авторы", "致谢", "クレジット", "المساهمون", "קרדיטים"),
    "Components": ("Componentes", "Componenti", "Composants", "Komponenten", "Składniki", "Компоненты", "组件", "コンポーネント", "المكونات", "רכיבים"),
    "License": ("Licencia", "Licenza", "Licence", "Lizenz", "Licencja", "Лицензия", "许可证", "ライセンス", "الترخيص", "רישיון"),
    "Built with": ("Construido con", "Realizzato con", "Construit avec", "Erstellt mit", "Zbudowano przy użyciu", "Создано с помощью", "构建所用", "使用ライブラリ", "بُني باستخدام", "נבנה באמצעות"),
    "Format driver": ("Controlador de formatos", "Driver dei formati", "Pilote de formats", "Formattreiber", "Sterownik formatów", "Драйвер форматов", "格式驱动", "フォーマットドライバー", "مشغّل الصيغ", "מנהל התקן פורמטים"),
    "System": ("Sistema", "Sistema", "Système", "System", "System", "Система", "系统", "システム", "النظام", "מערכת"),
    "Dragon UnPACKer: the original author": ("Dragon UnPACKer: el autor original", "Dragon UnPACKer: l'autore originale", "Dragon UnPACKer : l'auteur original", "Dragon UnPACKer: der ursprüngliche Autor", "Dragon UnPACKer: oryginalny autor", "Dragon UnPACKer: оригинальный автор", "Dragon UnPACKer：原作者", "Dragon UnPACKer：原作者", "Dragon UnPACKer: المؤلف الأصلي", "Dragon UnPACKer: היוצר המקורי"),
    "Dragon UnPACKer++: port and new features": ("Dragon UnPACKer++: port y nuevas funciones", "Dragon UnPACKer++: porting e nuove funzioni", "Dragon UnPACKer++ : portage et nouvelles fonctions", "Dragon UnPACKer++: Portierung und neue Funktionen", "Dragon UnPACKer++: port i nowe funkcje", "Dragon UnPACKer++: порт и новые возможности", "Dragon UnPACKer++：移植与新功能", "Dragon UnPACKer++：移植と新機能", "Dragon UnPACKer++: النقل والميزات الجديدة", "Dragon UnPACKer++: הסבה ותכונות חדשות"),
    "Original project: %1": ("Proyecto original: %1", "Progetto originale: %1", "Projet d'origine : %1", "Ursprüngliches Projekt: %1", "Oryginalny projekt: %1", "Оригинальный проект: %1", "原项目：%1", "元のプロジェクト：%1", "المشروع الأصلي: %1", "הפרויקט המקורי: %1"),
    "A fast explorer and extractor for the resource files of games: it opens the archives, shows what is inside, previews pictures, sounds, text and 3D models, and extracts them, converting what is needed.": (
        "Un explorador y extractor rápido de los archivos de recursos de los juegos: abre los paquetes, muestra su contenido, previsualiza imágenes, sonidos, texto y modelos 3D, y los extrae convirtiendo lo necesario.",
        "Un esploratore ed estrattore veloce dei file di risorse dei giochi: apre gli archivi, mostra il contenuto, visualizza immagini, suoni, testo e modelli 3D, e li estrae convertendo ciò che serve.",
        "Un explorateur et extracteur rapide des fichiers de ressources des jeux : il ouvre les archives, montre leur contenu, prévisualise images, sons, textes et modèles 3D, et les extrait en convertissant ce qu'il faut.",
        "Ein schneller Explorer und Extraktor für die Ressourcendateien von Spielen: Er öffnet Archive, zeigt den Inhalt, zeigt Bilder, Klänge, Text und 3D-Modelle in der Vorschau und extrahiert sie, wobei er bei Bedarf konvertiert.",
        "Szybki eksplorator i ekstraktor plików zasobów gier: otwiera archiwa, pokazuje ich zawartość, podgląda obrazy, dźwięki, tekst i modele 3D oraz wypakowuje je, konwertując w razie potrzeby.",
        "Быстрый обозреватель и экстрактор файлов ресурсов игр: открывает архивы, показывает содержимое, показывает предпросмотр изображений, звуков, текста и 3D-моделей и извлекает их, при необходимости преобразуя.",
        "游戏资源文件的快速浏览与提取工具：打开归档，显示其内容，预览图像、声音、文本和 3D 模型，并在需要时转换后提取。",
        "ゲームのリソースファイル用の高速なエクスプローラー兼展開ツール。アーカイブを開いて中身を表示し、画像・サウンド・テキスト・3D モデルをプレビューし、必要に応じて変換して展開します。",
        "مستكشف ومستخرج سريع لملفات موارد الألعاب: يفتح الأرشيفات ويعرض محتواها ويعاين الصور والأصوات والنصوص والنماذج ثلاثية الأبعاد ويستخرجها مع تحويل ما يلزم.",
        "סייר ומחלץ מהיר לקובצי משאבים של משחקים: פותח ארכיונים, מציג את תוכנם, מציג תצוגה מקדימה של תמונות, צלילים, טקסט ומודלים תלת־ממדיים, ומחלץ אותם תוך המרה במידת הצורך."),
    "It continues Dragon UnPACKer, which Alexandre \"Elbereth\" Devilliers created and developed for many years: the same idea and the same formats, now in C++ with a Qt 6 interface, running on Linux, and with many more formats.": (
        "Continúa Dragon UnPACKer, que Alexandre \"Elbereth\" Devilliers creó y desarrolló durante muchos años: la misma idea y los mismos formatos, ahora en C++ con interfaz Qt 6, funcionando en Linux y con muchos más formatos.",
        "Continua Dragon UnPACKer, che Alexandre \"Elbereth\" Devilliers ha creato e sviluppato per molti anni: la stessa idea e gli stessi formati, ora in C++ con interfaccia Qt 6, per Linux e con molti più formati.",
        "Il poursuit Dragon UnPACKer, créé et développé pendant de nombreuses années par Alexandre « Elbereth » Devilliers : la même idée et les mêmes formats, désormais en C++ avec une interface Qt 6, sous Linux, et avec beaucoup plus de formats.",
        "Es führt Dragon UnPACKer fort, das Alexandre „Elbereth“ Devilliers viele Jahre lang geschaffen und entwickelt hat: dieselbe Idee und dieselben Formate, jetzt in C++ mit Qt-6-Oberfläche, unter Linux und mit sehr vielen weiteren Formaten.",
        "Kontynuuje Dragon UnPACKer, który Alexandre „Elbereth” Devilliers tworzył i rozwijał przez wiele lat: ten sam pomysł i te same formaty, teraz w C++ z interfejsem Qt 6, działający w Linuksie i z wieloma dodatkowymi formatami.",
        "Это продолжение Dragon UnPACKer, который Александр «Elbereth» Девильер (Alexandre Devilliers) создавал и развивал много лет: та же идея и те же форматы, теперь на C++ с интерфейсом Qt 6, для Linux и с гораздо большим числом форматов.",
        "它延续了 Alexandre “Elbereth” Devilliers 多年创建和开发的 Dragon UnPACKer：相同的理念和格式，现在使用 C++ 和 Qt 6 界面，可在 Linux 上运行，并支持多得多的格式。",
        "Alexandre “Elbereth” Devilliers 氏が長年作り育ててきた Dragon UnPACKer の後継です。同じ発想と同じフォーマットを、C++ と Qt 6 のインターフェースで Linux 上に実現し、対応フォーマットを大幅に増やしました。",
        "يواصل مشروع Dragon UnPACKer الذي أنشأه Alexandre \"Elbereth\" Devilliers وطوّره سنوات طويلة: الفكرة نفسها والصيغ نفسها، الآن بلغة C++ وواجهة Qt 6 على لينكس ومع صيغ أكثر بكثير.",
        "ממשיך את Dragon UnPACKer, שאותו יצר ופיתח Alexandre ‏\"Elbereth\" Devilliers שנים רבות: אותו רעיון ואותם פורמטים, עכשיו ב־C++ עם ממשק Qt 6, על לינוקס ועם הרבה יותר פורמטים."),
    "Created Dragon UnPACKer and wrote its versions 1 to 5 in Delphi, with the plugins, the formats and the HyperRipper that this program is built on.": (
        "Creó Dragon UnPACKer y escribió sus versiones 1 a 5 en Delphi, con los complementos, los formatos y el HyperRipper en los que se basa este programa.",
        "Ha creato Dragon UnPACKer e ne ha scritto le versioni da 1 a 5 in Delphi, con i plugin, i formati e l'HyperRipper su cui si basa questo programma.",
        "A créé Dragon UnPACKer et écrit ses versions 1 à 5 en Delphi, avec les plugins, les formats et le HyperRipper sur lesquels ce programme repose.",
        "Schuf Dragon UnPACKer und schrieb die Versionen 1 bis 5 in Delphi, mit den Plugins, den Formaten und dem HyperRipper, auf denen dieses Programm aufbaut.",
        "Stworzył Dragon UnPACKer i napisał jego wersje od 1 do 5 w Delphi, wraz z wtyczkami, formatami i HyperRipperem, na których opiera się ten program.",
        "Создал Dragon UnPACKer и написал его версии с 1 по 5 на Delphi, включая плагины, форматы и HyperRipper, на которых основана эта программа.",
        "创建了 Dragon UnPACKer，并用 Delphi 编写了 1 至 5 版，包括本程序所依据的插件、格式和 HyperRipper。",
        "Dragon UnPACKer を作り、バージョン 1〜5 を Delphi で開発しました。本プログラムの土台となるプラグイン、フォーマット、HyperRipper も彼の作品です。",
        "أنشأ Dragon UnPACKer وكتب إصداراته من 1 إلى 5 بلغة Delphi، مع الإضافات والصيغ وأداة HyperRipper التي يقوم عليها هذا البرنامج.",
        "יצר את Dragon UnPACKer וכתב את גרסאות 1 עד 5 ב־Delphi, עם התוספים, הפורמטים וה־HyperRipper שעליהם נשענת תוכנה זו."),
    "Ported the program to C++20 and Qt 6 and the formats to a new driver, and added the new formats, the Unity, FMOD and XNA readers, the previews, the file type identification, the translations and the other new features.": (
        "Portó el programa a C++20 y Qt 6 y los formatos a un nuevo controlador, y añadió los nuevos formatos, los lectores de Unity, FMOD y XNA, las vistas previas, la identificación de tipos de archivo, las traducciones y otras funciones nuevas.",
        "Ha portato il programma in C++20 e Qt 6 e i formati in un nuovo driver, e ha aggiunto i nuovi formati, i lettori Unity, FMOD e XNA, le anteprime, l'identificazione dei tipi di file, le traduzioni e le altre nuove funzioni.",
        "A porté le programme en C++20 et Qt 6 et les formats dans un nouveau pilote, et ajouté les nouveaux formats, les lecteurs Unity, FMOD et XNA, les aperçus, l'identification des types de fichier, les traductions et les autres nouvelles fonctions.",
        "Portierte das Programm nach C++20 und Qt 6 und die Formate in einen neuen Treiber und fügte die neuen Formate, die Unity-, FMOD- und XNA-Leser, die Vorschau, die Dateityp-Erkennung, die Übersetzungen und weitere neue Funktionen hinzu.",
        "Przeniósł program do C++20 i Qt 6, a formaty do nowego sterownika, oraz dodał nowe formaty, czytniki Unity, FMOD i XNA, podglądy, rozpoznawanie typów plików, tłumaczenia i inne nowe funkcje.",
        "Перенёс программу на C++20 и Qt 6, а форматы — в новый драйвер, и добавил новые форматы, чтение Unity, FMOD и XNA, предпросмотр, определение типов файлов, переводы и другие новые возможности.",
        "将程序移植到 C++20 和 Qt 6，将格式移植到新的驱动，并新增了格式、Unity、FMOD 和 XNA 读取器、预览、文件类型识别、翻译及其他新功能。",
        "プログラムを C++20 と Qt 6 に、フォーマットを新しいドライバーに移植し、新フォーマット、Unity・FMOD・XNA の読み込み、プレビュー、ファイル種類の判別、翻訳などの新機能を追加しました。",
        "نقل البرنامج إلى C++20 وQt 6 ونقل الصيغ إلى مشغّل جديد، وأضاف صيغاً جديدة وقراءة Unity وFMOD وXNA والمعاينات والتعرف على أنواع الملفات والترجمات وميزات جديدة أخرى.",
        "הסב את התוכנה ל־C++20 ול־Qt 6 ואת הפורמטים למנהל התקן חדש, והוסיף פורמטים חדשים, קוראי Unity, FMOD ו־XNA, תצוגות מקדימות, זיהוי סוגי קבצים, תרגומים ותכונות חדשות נוספות."),
    "Thanks to everyone who contributed formats, plugins, translations and bug reports to Dragon UnPACKer over the years.": (
        "Gracias a todos los que aportaron formatos, complementos, traducciones e informes de errores a Dragon UnPACKer a lo largo de los años.",
        "Grazie a tutti coloro che hanno contribuito a Dragon UnPACKer con formati, plugin, traduzioni e segnalazioni di errori nel corso degli anni.",
        "Merci à tous ceux qui ont apporté à Dragon UnPACKer, au fil des ans, des formats, des plugins, des traductions et des rapports de bogues.",
        "Dank an alle, die Dragon UnPACKer über die Jahre Formate, Plugins, Übersetzungen und Fehlerberichte beigesteuert haben.",
        "Podziękowania dla wszystkich, którzy na przestrzeni lat wnieśli do Dragon UnPACKer formaty, wtyczki, tłumaczenia i zgłoszenia błędów.",
        "Спасибо всем, кто на протяжении многих лет вносил в Dragon UnPACKer форматы, плагины, переводы и отчёты об ошибках.",
        "感谢多年来为 Dragon UnPACKer 贡献格式、插件、翻译和错误报告的所有人。",
        "長年にわたり Dragon UnPACKer にフォーマット、プラグイン、翻訳、バグ報告を寄せてくださったすべての方々に感謝します。",
        "شكراً لكل من ساهم عبر السنين في Dragon UnPACKer بالصيغ والإضافات والترجمات وتقارير الأخطاء.",
        "תודה לכל מי שתרם ל־Dragon UnPACKer לאורך השנים פורמטים, תוספים, תרגומים ודיווחי באגים."),
    "Decoders written for this program, some after public descriptions: RNC, LZO, LZMA, LZ4, LZX, crunch (from Unity's fork, zlib license), ETC, ASTC, BC1 to BC7, Vorbis to Ogg": (
        "Decodificadores escritos para este programa, algunos a partir de descripciones públicas: RNC, LZO, LZMA, LZ4, LZX, crunch (del fork de Unity, licencia zlib), ETC, ASTC, BC1 a BC7, Vorbis a Ogg",
        "Decodificatori scritti per questo programma, alcuni da descrizioni pubbliche: RNC, LZO, LZMA, LZ4, LZX, crunch (dal fork di Unity, licenza zlib), ETC, ASTC, da BC1 a BC7, Vorbis in Ogg",
        "Décodeurs écrits pour ce programme, certains d'après des descriptions publiques : RNC, LZO, LZMA, LZ4, LZX, crunch (du fork d'Unity, licence zlib), ETC, ASTC, BC1 à BC7, Vorbis vers Ogg",
        "Für dieses Programm geschriebene Decoder, teils nach öffentlichen Beschreibungen: RNC, LZO, LZMA, LZ4, LZX, Crunch (aus dem Unity-Fork, zlib-Lizenz), ETC, ASTC, BC1 bis BC7, Vorbis zu Ogg",
        "Dekodery napisane dla tego programu, część według publicznych opisów: RNC, LZO, LZMA, LZ4, LZX, crunch (z forka Unity, licencja zlib), ETC, ASTC, BC1 do BC7, Vorbis do Ogg",
        "Декодеры, написанные для этой программы, часть по открытым описаниям: RNC, LZO, LZMA, LZ4, LZX, crunch (из форка Unity, лицензия zlib), ETC, ASTC, BC1–BC7, Vorbis в Ogg",
        "为本程序编写的解码器，部分依据公开说明：RNC、LZO、LZMA、LZ4、LZX、crunch（来自 Unity 的分支，zlib 许可证）、ETC、ASTC、BC1 至 BC7、Vorbis 转 Ogg",
        "本プログラム用に作成したデコーダー（一部は公開された仕様に基づく）：RNC、LZO、LZMA、LZ4、LZX、crunch（Unity のフォーク、zlib ライセンス）、ETC、ASTC、BC1〜BC7、Vorbis から Ogg",
        "مفككات ترميز كُتبت لهذا البرنامج، بعضها وفق أوصاف عامة: RNC وLZO وLZMA وLZ4 وLZX وcrunch (من نسخة Unity المتفرعة، ترخيص zlib) وETC وASTC وBC1 إلى BC7 وVorbis إلى Ogg",
        "מפענחים שנכתבו עבור תוכנה זו, חלקם לפי תיאורים פומביים: RNC, ‏LZO, ‏LZMA, ‏LZ4, ‏LZX, ‏crunch (מהפיצול של Unity, רישיון zlib), ‏ETC, ‏ASTC, ‏BC1 עד BC7, ‏Vorbis ל־Ogg"),
    "This program is free software. It is distributed under the terms of the Mozilla Public License, version 2.0.": (
        "Este programa es software libre. Se distribuye bajo los términos de la Mozilla Public License, versión 2.0.",
        "Questo programma è software libero. È distribuito secondo i termini della Mozilla Public License, versione 2.0.",
        "Ce programme est un logiciel libre. Il est distribué selon les termes de la Mozilla Public License, version 2.0.",
        "Dieses Programm ist freie Software. Es wird unter den Bedingungen der Mozilla Public License, Version 2.0, verbreitet.",
        "Ten program jest wolnym oprogramowaniem. Jest rozpowszechniany na warunkach Mozilla Public License w wersji 2.0.",
        "Эта программа — свободное ПО. Она распространяется на условиях Mozilla Public License версии 2.0.",
        "本程序是自由软件，按 Mozilla Public License 2.0 版的条款分发。",
        "このプログラムはフリーソフトウェアです。Mozilla Public License バージョン 2.0 の条件で配布されています。",
        "هذا البرنامج حر. يُوزَّع بموجب شروط رخصة Mozilla العامة، الإصدار 2.0.",
        "תוכנה זו היא תוכנה חופשית. היא מופצת בהתאם לתנאי Mozilla Public License גרסה 2.0."),
    "It is provided as it is, without any warranty. The files and the games that you open with it belong to their owners.": (
        "Se ofrece tal cual, sin ninguna garantía. Los archivos y los juegos que abra con ella pertenecen a sus propietarios.",
        "È fornito così com'è, senza alcuna garanzia. I file e i giochi che apri con esso appartengono ai rispettivi proprietari.",
        "Il est fourni tel quel, sans aucune garantie. Les fichiers et les jeux que vous ouvrez avec appartiennent à leurs propriétaires.",
        "Es wird ohne jegliche Gewähr bereitgestellt. Die Dateien und Spiele, die Sie damit öffnen, gehören ihren Eigentümern.",
        "Program jest dostarczany w stanie, w jakim jest, bez żadnej gwarancji. Pliki i gry otwierane za jego pomocą należą do ich właścicieli.",
        "Программа предоставляется «как есть», без каких-либо гарантий. Файлы и игры, которые вы открываете с её помощью, принадлежат их владельцам.",
        "本程序按“现状”提供，不附带任何担保。您用它打开的文件和游戏归其所有者所有。",
        "本プログラムは現状のまま提供され、いかなる保証もありません。開いたファイルやゲームは、それぞれの権利者のものです。",
        "يُقدَّم كما هو دون أي ضمان. الملفات والألعاب التي تفتحها به مملوكة لأصحابها.",
        "התוכנה מסופקת כפי שהיא, ללא כל אחריות. הקבצים והמשחקים שאתם פותחים בה שייכים לבעליהם."),
    "<p>%1: %2 formats<br><small>%3</small></p>": (
        "<p>%1: %2 formatos<br><small>%3</small></p>", "<p>%1: %2 formati<br><small>%3</small></p>", "<p>%1 : %2 formats<br><small>%3</small></p>", "<p>%1: %2 Formate<br><small>%3</small></p>", "<p>%1: %2 formatów<br><small>%3</small></p>",
        "<p>%1: форматов — %2<br><small>%3</small></p>", "<p>%1：%2 种格式<br><small>%3</small></p>", "<p>%1：%2 フォーマット<br><small>%3</small></p>", "<p>%1: %2 صيغة<br><small>%3</small></p>", "<p>%1: %2 פורמטים<br><small>%3</small></p>"),
})

PT_BR.update({
    "About": "Sobre",
    "Explore, preview and extract the resources of games": "Explore, visualize e extraia os recursos dos jogos",
    "Version %1": "Versão %1", "Credits": "Créditos", "Components": "Componentes", "License": "Licença", "Built with": "Construído com", "Format driver": "Driver de formatos", "System": "Sistema",
    "Dragon UnPACKer: the original author": "Dragon UnPACKer: o autor original",
    "Dragon UnPACKer++: port and new features": "Dragon UnPACKer++: port e novos recursos",
    "Original project: %1": "Projeto original: %1",
    "A fast explorer and extractor for the resource files of games: it opens the archives, shows what is inside, previews pictures, sounds, text and 3D models, and extracts them, converting what is needed.": "Um explorador e extrator rápido dos arquivos de recursos de jogos: abre os pacotes, mostra o conteúdo, visualiza imagens, sons, texto e modelos 3D, e os extrai convertendo o que for preciso.",
    "It continues Dragon UnPACKer, which Alexandre \"Elbereth\" Devilliers created and developed for many years: the same idea and the same formats, now in C++ with a Qt 6 interface, running on Linux, and with many more formats.": "Continua o Dragon UnPACKer, que Alexandre \"Elbereth\" Devilliers criou e desenvolveu por muitos anos: a mesma ideia e os mesmos formatos, agora em C++ com interface Qt 6, rodando no Linux e com muito mais formatos.",
    "Created Dragon UnPACKer and wrote its versions 1 to 5 in Delphi, with the plugins, the formats and the HyperRipper that this program is built on.": "Criou o Dragon UnPACKer e escreveu as versões 1 a 5 em Delphi, com os plugins, os formatos e o HyperRipper sobre os quais este programa é construído.",
    "Ported the program to C++20 and Qt 6 and the formats to a new driver, and added the new formats, the Unity, FMOD and XNA readers, the previews, the file type identification, the translations and the other new features.": "Portou o programa para C++20 e Qt 6 e os formatos para um novo driver, e acrescentou os novos formatos, os leitores de Unity, FMOD e XNA, as visualizações, a identificação de tipos de arquivo, as traduções e os demais recursos novos.",
    "Thanks to everyone who contributed formats, plugins, translations and bug reports to Dragon UnPACKer over the years.": "Obrigado a todos que contribuíram com formatos, plugins, traduções e relatos de erros para o Dragon UnPACKer ao longo dos anos.",
    "Decoders written for this program, some after public descriptions: RNC, LZO, LZMA, LZ4, LZX, crunch (from Unity's fork, zlib license), ETC, ASTC, BC1 to BC7, Vorbis to Ogg": "Decodificadores escritos para este programa, alguns a partir de descrições públicas: RNC, LZO, LZMA, LZ4, LZX, crunch (do fork da Unity, licença zlib), ETC, ASTC, BC1 a BC7, Vorbis para Ogg",
    "This program is free software. It is distributed under the terms of the Mozilla Public License, version 2.0.": "Este programa é software livre. É distribuído sob os termos da Mozilla Public License, versão 2.0.",
    "It is provided as it is, without any warranty. The files and the games that you open with it belong to their owners.": "É fornecido como está, sem nenhuma garantia. Os arquivos e jogos que você abre com ele pertencem aos seus donos.",
    "<p>%1: %2 formats<br><small>%3</small></p>": "<p>%1: %2 formatos<br><small>%3</small></p>",
})


# The palettes found in a console ROM: the context menu of the entries and the status bar
TRANSLATIONS.update({
    "Use as palette for previews": (
        "Usar como paleta para las vistas previas", "Usa come tavolozza per le anteprime", "Utiliser comme palette pour les aperçus", "Als Palette für Vorschauen verwenden",
        "Użyj jako palety podglądu", "Использовать как палитру для предпросмотра", "用作预览调色板", "プレビューのパレットとして使用", "استخدام كلوحة ألوان للمعاينة", "השתמש כפלטה לתצוגה מקדימה"),
    "Show the pictures in their own colors": (
        "Mostrar las imágenes con sus propios colores", "Mostra le immagini con i loro colori", "Afficher les images avec leurs propres couleurs", "Bilder in ihren eigenen Farben anzeigen",
        "Pokaż obrazy w ich własnych kolorach", "Показывать изображения в собственных цветах", "以图像自身的颜色显示", "画像を元の色で表示", "عرض الصور بألوانها الأصلية", "הצג תמונות בצבעים המקוריים שלהן"),
    "Previews use the palette %1": (
        "Las vistas previas usan la paleta %1", "Le anteprime usano la tavolozza %1", "Les aperçus utilisent la palette %1", "Vorschauen verwenden die Palette %1",
        "Podgląd używa palety %1", "Предпросмотр использует палитру %1", "预览使用调色板 %1", "プレビューはパレット %1 を使用します", "تستخدم المعاينة لوحة الألوان %1", "התצוגה המקדימה משתמשת בפלטה %1"),
})
PT_BR.update({
    "Use as palette for previews": "Usar como paleta para as visualizações",
    "Show the pictures in their own colors": "Mostrar as imagens com as cores originais",
    "Previews use the palette %1": "As visualizações usam a paleta %1",
})


# The type filter of the entry list, the System theme, the zoom buttons of the previews and the model viewers
TRANSLATIONS.update({
    "Show only the files of one type (the extension)": (
        "Mostrar solo los archivos de un tipo (la extensión)", "Mostra solo i file di un tipo (l'estensione)", "Afficher seulement les fichiers d'un type (l'extension)", "Nur die Dateien eines Typs (der Erweiterung) anzeigen", "Pokaż tylko pliki jednego typu (rozszerzenia)", "Показывать только файлы одного типа (расширения)",
        "只显示一种类型（扩展名）的文件", "1 種類（拡張子）のファイルだけを表示", "إظهار ملفات نوع واحد فقط (الامتداد)", "הצג רק את הקבצים מסוג אחד (הסיומת)"),
    "All types": (
        "Todos los tipos", "Tutti i tipi", "Tous les types", "Alle Typen", "Wszystkie typy", "Все типы",
        "所有类型", "すべての種類", "كل الأنواع", "כל הסוגים"),
    "(no extension)": (
        "(sin extensión)", "(nessuna estensione)", "(sans extension)", "(keine Erweiterung)", "(bez rozszerzenia)", "(без расширения)",
        "（无扩展名）", "（拡張子なし）", "(بلا امتداد)", "(ללא סיומת)"),
    "Animate": (
        "Animar", "Anima", "Animer", "Animieren", "Animuj", "Анимировать",
        "动画", "アニメーション", "تحريك", "הנפש"),
    "Off: the model as it is drawn without animation. On: the animation tracks at the position of the slider.": (
        "Desactivado: el modelo tal como se dibuja sin animación. Activado: las pistas de animación en la posición del control deslizante.", "Disattivato: il modello come viene disegnato senza animazione. Attivato: le tracce di animazione alla posizione del cursore.", "Désactivé : le modèle tel qu'il est dessiné sans animation. Activé : les pistes d'animation à la position du curseur.", "Aus: das Modell, wie es ohne Animation gezeichnet wird. Ein: die Animationsspuren an der Position des Schiebereglers.", "Wyłączone: model tak, jak jest rysowany bez animacji. Włączone: ścieżki animacji w położeniu suwaka.", "Выключено: модель так, как она рисуется без анимации. Включено: дорожки анимации в положении ползунка.",
        "关闭：按无动画时的样子绘制模型。开启：显示滑块位置处的动画轨道。", "オフ：アニメーションなしで描画されるモデル。オン：スライダーの位置のアニメーショントラック。", "معطّل: النموذج كما يُرسم بلا تحريك. مفعّل: مسارات التحريك عند موضع شريط التمرير.", "כבוי: המודל כפי שהוא מצויר ללא הנפשה. פועל: רצועות ההנפשה במיקום המחוון."),
    "rest pose": (
        "pose de reposo", "posa di riposo", "pose de repos", "Ruhepose", "poza spoczynkowa", "поза покоя",
        "静止姿势", "基本ポーズ", "وضعية السكون", "תנוחת מנוחה"),
    "Speed of the playback (one pass of the animation takes 4 seconds at 1x; the $rotate parts follow it)": (
        "Velocidad de reproducción (una pasada de la animación dura 4 segundos a 1x; las partes $rotate la siguen)", "Velocità di riproduzione (un ciclo dell'animazione dura 4 secondi a 1x; le parti $rotate la seguono)", "Vitesse de lecture (un passage de l'animation dure 4 secondes à 1x ; les parties $rotate la suivent)", "Wiedergabegeschwindigkeit (ein Durchlauf der Animation dauert bei 1x 4 Sekunden; die $rotate-Teile folgen ihr)", "Szybkość odtwarzania (jedno przejście animacji trwa 4 sekundy przy 1x; części $rotate podążają za nią)", "Скорость воспроизведения (один проход анимации длится 4 секунды при 1x; части $rotate следуют за ней)",
        "播放速度（1x 时动画一次播放需 4 秒；$rotate 部件随之转动）", "再生速度（1x でアニメーション 1 回は 4 秒；$rotate の部品もそれに従います）", "سرعة التشغيل (تستغرق دورة التحريك 4 ثوانٍ عند 1x؛ وتتبعها أجزاء $rotate)", "מהירות הניגון (מעבר אחד של ההנפשה נמשך 4 שניות ב־1x; חלקי ה־$rotate עוקבים אחריה)"),
    "%1%": (
        "%1%", "%1%", "%1 %", "%1 %", "%1%", "%1 %",
        "%1%", "%1%", "%1%", "%1%"),
    "Model: %1 subobjects, %2 triangles": (
        "Modelo: %1 subobjetos, %2 triángulos", "Modello: %1 sotto-oggetti, %2 triangoli", "Modèle : %1 sous-objets, %2 triangles", "Modell: %1 Unterobjekte, %2 Dreiecke", "Model: podobiektów %1, trójkątów %2", "Модель: подобъектов %1, треугольников %2",
        "模型：%1 个子对象，%2 个三角形", "モデル：サブオブジェクト %1、三角形 %2", "النموذج: %1 كائن فرعي، %2 مثلث", "מודל: %1 תת־עצמים, %2 משולשים"),
    "As the system": (
        "Como el sistema", "Come il sistema", "Comme le système", "Wie das System", "Jak w systemie", "Как в системе",
        "跟随系统", "システムに合わせる", "مثل النظام", "כמו המערכת"),
    "Zoom out (Ctrl+wheel down, -)": (
        "Alejar (Ctrl+rueda abajo, -)", "Riduci (Ctrl+rotella giù, -)", "Zoom arrière (Ctrl+molette vers le bas, -)", "Verkleinern (Strg+Mausrad nach unten, -)", "Pomniejsz (Ctrl+kółko w dół, -)", "Уменьшить (Ctrl+колёсико вниз, -)",
        "缩小（Ctrl+滚轮向下，-）", "縮小（Ctrl+ホイール下、-）", "تصغير (‏Ctrl+العجلة للأسفل، -)", "התרחק (‏Ctrl+גלגלת למטה, -)"),
    "Zoom in (Ctrl+wheel up, +)": (
        "Acercar (Ctrl+rueda arriba, +)", "Ingrandisci (Ctrl+rotella su, +)", "Zoom avant (Ctrl+molette vers le haut, +)", "Vergrößern (Strg+Mausrad nach oben, +)", "Powiększ (Ctrl+kółko w górę, +)", "Увеличить (Ctrl+колёсико вверх, +)",
        "放大（Ctrl+滚轮向上，+）", "拡大（Ctrl+ホイール上、+）", "تكبير (‏Ctrl+العجلة للأعلى، +)", "התקרב (‏Ctrl+גלגלת למעלה, +)"),
    "Actual size (0)": (
        "Tamaño real (0)", "Dimensione reale (0)", "Taille réelle (0)", "Originalgröße (0)", "Rzeczywisty rozmiar (0)", "Реальный размер (0)",
        "实际大小（0）", "実際のサイズ（0）", "الحجم الفعلي (‏0)", "גודל אמיתי (‏0)"),
    "Fit": (
        "Ajustar", "Adatta", "Ajuster", "Einpassen", "Dopasuj", "Вписать",
        "适应", "合わせる", "ملاءمة", "התאם"),
    "Fit to the panel (F)": (
        "Ajustar al panel (F)", "Adatta al pannello (F)", "Ajuster au panneau (F)", "In das Feld einpassen (F)", "Dopasuj do panelu (F)", "Вписать в панель (F)",
        "适应面板（F）", "パネルに合わせる（F）", "ملاءمة اللوحة (‏F)", "התאם ללוח (‏F)"),
    "The animation to show": (
        "La animación que se muestra", "L'animazione da mostrare", "L'animation à afficher", "Die anzuzeigende Animation", "Animacja do pokazania", "Показываемая анимация",
        "要显示的动画", "表示するアニメーション", "التحريك المعروض", "ההנפשה להצגה"),
    "Speed of the playback (1x is the speed of the game, 25 frames per second)": (
        "Velocidad de reproducción (1x es la velocidad del juego, 25 fotogramas por segundo)", "Velocità di riproduzione (1x è la velocità del gioco, 25 frame al secondo)", "Vitesse de lecture (1x est la vitesse du jeu, 25 trames par seconde)", "Wiedergabegeschwindigkeit (1x ist die Geschwindigkeit des Spiels, 25 Frames pro Sekunde)", "Szybkość odtwarzania (1x to szybkość gry, 25 klatek na sekundę)", "Скорость воспроизведения (1x — скорость игры, 25 кадров в секунду)",
        "播放速度（1x 为游戏速度，每秒 25 帧）", "再生速度（1x はゲームの速度、毎秒 25 フレーム）", "سرعة التشغيل (‏1x هي سرعة اللعبة، 25 إطاراً في الثانية)", "מהירות הניגון (‏1x היא מהירות המשחק, 25 פריימים בשנייה)"),
    "animation %1": (
        "animación %1", "animazione %1", "animation %1", "Animation %1", "animacja %1", "анимация %1",
        "动画 %1", "アニメーション %1", "التحريك %1", "הנפשה %1"),
    "frame %1 / %2": (
        "fotograma %1 / %2", "frame %1 / %2", "trame %1 / %2", "Frame %1 / %2", "klatka %1 / %2", "кадр %1 / %2",
        "帧 %1 / %2", "フレーム %1 / %2", "الإطار %1 / %2", "פריים %1 / %2"),
    "Model: %1 limbs, %2 triangles, %3 animations": (
        "Modelo: %1 miembros, %2 triángulos, %3 animaciones", "Modello: %1 arti, %2 triangoli, %3 animazioni", "Modèle : %1 membres, %2 triangles, %3 animations", "Modell: %1 Glieder, %2 Dreiecke, %3 Animationen", "Model: kończyn %1, trójkątów %2, animacji %3", "Модель: конечностей %1, треугольников %2, анимаций %3",
        "模型：%1 个肢体，%2 个三角形，%3 个动画", "モデル：リム %1、三角形 %2、アニメーション %3", "النموذج: %1 طرف، %2 مثلث، %3 تحريك", "מודל: %1 איברים, %2 משולשים, %3 הנפשות"),
})
PT_BR.update({
    "Show only the files of one type (the extension)": "Mostrar só os arquivos de um tipo (a extensão)",
    "All types": "Todos os tipos",
    "(no extension)": "(sem extensão)",
    "Animate": "Animar",
    "Off: the model as it is drawn without animation. On: the animation tracks at the position of the slider.": "Desligado: o modelo como é desenhado sem animação. Ligado: as trilhas de animação na posição do controle deslizante.",
    "rest pose": "pose de repouso",
    "Speed of the playback (one pass of the animation takes 4 seconds at 1x; the $rotate parts follow it)": "Velocidade da reprodução (uma passada da animação leva 4 segundos em 1x; as partes $rotate a acompanham)",
    "%1%": "%1%",
    "Model: %1 subobjects, %2 triangles": "Modelo: %1 subobjetos, %2 triângulos",
    "As the system": "Como o sistema",
    "Zoom out (Ctrl+wheel down, -)": "Diminuir o zoom (Ctrl+roda para baixo, -)",
    "Zoom in (Ctrl+wheel up, +)": "Aumentar o zoom (Ctrl+roda para cima, +)",
    "Actual size (0)": "Tamanho real (0)",
    "Fit": "Ajustar",
    "Fit to the panel (F)": "Ajustar ao painel (F)",
    "The animation to show": "A animação a mostrar",
    "Speed of the playback (1x is the speed of the game, 25 frames per second)": "Velocidade da reprodução (1x é a velocidade do jogo, 25 quadros por segundo)",
    "animation %1": "animação %1",
    "frame %1 / %2": "quadro %1 / %2",
    "Model: %1 limbs, %2 triangles, %3 animations": "Modelo: %1 membros, %2 triângulos, %3 animações",
})

# the controls of the 3D viewers (Ui/ViewNavigation) and their help
TRANSLATIONS.update({
    "Wireframe on or off": (
        "Malla de alambre activada o desactivada", "Wireframe attivato o disattivato", "Fil de fer activé ou désactivé", "Drahtgitter ein oder aus", "Siatka włączona lub wyłączona", "Каркас вкл. или выкл.",
        "开启或关闭线框", "ワイヤーフレームのオン／オフ", "تشغيل الإطار السلكي أو إيقافه", "הפעלה או כיבוי של מסגרת תיל"),
    "Frame the model again (the first view)": (
        "Encuadrar de nuevo el modelo (la vista inicial)", "Inquadrare di nuovo il modello (la vista iniziale)", "Recadrer le modèle (la vue de départ)", "Modell wieder einpassen (die erste Ansicht)", "Ponownie wykadruj model (widok początkowy)", "Снова вписать модель (исходный вид)",
        "重新框住模型（初始视图）", "モデルを再び収める（最初の表示）", "إعادة تأطير النموذج (العرض الأول)", "מסגור מחדש של המודל (התצוגה הראשונה)"),
    "Back faces shown or hidden (Outrage models)": (
        "Caras traseras visibles u ocultas (modelos de Outrage)", "Facce posteriori mostrate o nascoste (modelli Outrage)", "Faces arrière affichées ou masquées (modèles Outrage)", "Rückseiten ein- oder ausblenden (Outrage-Modelle)", "Tylne ściany widoczne lub ukryte (modele Outrage)", "Показать или скрыть задние грани (модели Outrage)",
        "显示或隐藏背面（Outrage 模型）", "裏面の表示／非表示（Outrage のモデル）", "إظهار الأوجه الخلفية أو إخفاؤها (نماذج Outrage)", "הצגה או הסתרה של פאות אחוריות (מודלים של Outrage)"),
    "Space": ("Espacio", "Spazio", "Espace", "Leertaste", "Spacja", "Пробел", "空格键", "スペース", "مسافة", "רווח"),
    "Play or pause the animation (animated models)": (
        "Reproducir o pausar la animación (modelos animados)", "Riprodurre o mettere in pausa l'animazione (modelli animati)", "Lire ou mettre en pause l'animation (modèles animés)", "Animation abspielen oder anhalten (animierte Modelle)", "Odtwórz lub wstrzymaj animację (modele animowane)", "Воспроизвести или приостановить анимацию (анимированные модели)",
        "播放或暂停动画（动画模型）", "アニメーションの再生／一時停止（アニメーションするモデル）", "تشغيل التحريك أو إيقافه مؤقتاً (النماذج المتحركة)", "הפעלה או השהיה של ההנפשה (מודלים מונפשים)"),
    "3D viewer controls...": (
        "Controles del visor 3D...", "Comandi del visualizzatore 3D...", "Commandes de la visionneuse 3D...", "Steuerung des 3D-Betrachters...", "Sterowanie przeglądarką 3D...", "Управление 3D-просмотром...",
        "3D 查看器操作...", "3D ビューアーの操作...", "عناصر التحكم في عارض ثلاثي الأبعاد...", "פקדי מציג התלת־ממד..."),
    "Left button drag": (
        "Arrastrar con el botón izquierdo", "Trascinare con il tasto sinistro", "Glisser avec le bouton gauche", "Mit der linken Taste ziehen", "Przeciąganie lewym przyciskiem", "Перетаскивание левой кнопкой",
        "按住左键拖动", "左ボタンでドラッグ", "السحب بالزر الأيسر", "גרירה בלחצן השמאלי"),
    "Turn the model": ("Girar el modelo", "Ruotare il modello", "Tourner le modèle", "Modell drehen", "Obróć model", "Повернуть модель",
        "旋转模型", "モデルを回す", "تدوير النموذج", "סיבוב המודל"),
    "Right or middle button drag, or Shift + left button drag": (
        "Arrastrar con el botón derecho o central, o Mayús + botón izquierdo", "Trascinare con il tasto destro o centrale, o Maiusc + tasto sinistro", "Glisser avec le bouton droit ou central, ou Maj + bouton gauche", "Mit der rechten oder mittleren Taste ziehen, oder Umschalt + linke Taste", "Przeciąganie prawym lub środkowym przyciskiem albo Shift + lewym przyciskiem", "Перетаскивание правой или средней кнопкой, или Shift + левой кнопкой",
        "按住右键或中键拖动，或 Shift + 左键拖动", "右ボタンか中ボタンでドラッグ、または Shift + 左ボタンでドラッグ", "السحب بالزر الأيمن أو الأوسط، أو Shift + الزر الأيسر", "גרירה בלחצן הימני או האמצעי, או Shift + הלחצן השמאלי"),
    "Move the model": ("Mover el modelo", "Spostare il modello", "Déplacer le modèle", "Modell verschieben", "Przesuń model", "Переместить модель",
        "移动模型", "モデルを動かす", "تحريك النموذج", "הזזת המודל"),
    "Wheel": ("Rueda", "Rotella", "Molette", "Mausrad", "Kółko", "Колесо",
        "滚轮", "ホイール", "عجلة الفأرة", "גלגלת"),
    "Zoom in or out at the pointer": (
        "Acercar o alejar en el puntero", "Ingrandire o ridurre sul puntatore", "Zoomer ou dézoomer sous le pointeur", "Am Zeiger vergrößern oder verkleinern", "Przybliż lub oddal w miejscu wskaźnika", "Приблизить или отдалить у указателя",
        "在指针处放大或缩小", "ポインターの位置で拡大／縮小", "التكبير أو التصغير عند المؤشر", "התקרבות או התרחקות במקום הסמן"),
    "Arrow keys": ("Teclas de flecha", "Tasti freccia", "Touches fléchées", "Pfeiltasten", "Klawisze strzałek", "Клавиши со стрелками",
        "方向键", "矢印キー", "مفاتيح الأسهم", "מקשי החיצים"),
    "Zoom in or out": ("Acercar o alejar", "Ingrandire o ridurre", "Zoomer ou dézoomer", "Vergrößern oder verkleinern", "Przybliż lub oddal", "Приблизить или отдалить",
        "放大或缩小", "拡大／縮小", "التكبير أو التصغير", "התקרבות או התרחקות"),
    "This help": ("Esta ayuda", "Questa guida", "Cette aide", "Diese Hilfe", "Ta pomoc", "Эта справка",
        "本帮助", "このヘルプ", "هذه المساعدة", "עזרה זו"),
    "Click the view first: the keys go to the view that has the focus.": (
        "Haga clic primero en la vista: las teclas van a la vista que tiene el foco.", "Fare prima clic sulla vista: i tasti vanno alla vista che ha il focus.", "Cliquez d'abord dans la vue : les touches vont à la vue qui a le focus.", "Zuerst in die Ansicht klicken: die Tasten gehen an die Ansicht mit dem Fokus.", "Najpierw kliknij widok: klawisze trafiają do widoku, który ma fokus.", "Сначала щёлкните по виду: клавиши получает вид, у которого фокус.",
        "请先单击视图：按键会发送到拥有焦点的视图。", "先にビューをクリックしてください。キーはフォーカスのあるビューに送られます。", "انقر العرض أولاً: تذهب المفاتيح إلى العرض الذي عليه التركيز.", "לחצו קודם על התצוגה: המקשים נשלחים לתצוגה שבפוקוס."),
    "3D viewer controls": (
        "Controles del visor 3D", "Comandi del visualizzatore 3D", "Commandes de la visionneuse 3D", "Steuerung des 3D-Betrachters", "Sterowanie przeglądarką 3D", "Управление 3D-просмотром",
        "3D 查看器操作", "3D ビューアーの操作", "عناصر التحكم في عارض ثلاثي الأبعاد", "פקדי מציג התלת־ממד"),
    "H: help": ("H: ayuda", "H: guida", "H : aide", "H: Hilfe", "H: pomoc", "H: справка",
        "H：帮助", "H：ヘルプ", "H: مساعدة", "H: עזרה"),
})
PT_BR.update({
    "Wireframe on or off": "Aramado ligado ou desligado",
    "Frame the model again (the first view)": "Enquadrar o modelo de novo (a vista inicial)",
    "Back faces shown or hidden (Outrage models)": "Faces de trás visíveis ou ocultas (modelos do Outrage)",
    "Space": "Espaço",
    "Play or pause the animation (animated models)": "Tocar ou pausar a animação (modelos animados)",
    "3D viewer controls...": "&Controles do visualizador 3D...",
    "Left button drag": "Arrastar com o botão esquerdo",
    "Turn the model": "Girar o modelo",
    "Right or middle button drag, or Shift + left button drag": "Arrastar com o botão direito ou do meio, ou Shift + botão esquerdo",
    "Move the model": "Mover o modelo",
    "Wheel": "Roda do mouse",
    "Zoom in or out at the pointer": "Aproximar ou afastar no ponteiro",
    "Arrow keys": "Teclas de seta",
    "Zoom in or out": "Aproximar ou afastar",
    "This help": "Esta ajuda",
    "Click the view first: the keys go to the view that has the focus.": "Clique na vista primeiro: as teclas vão para a vista que tem o foco.",
    "3D viewer controls": "Controles do visualizador 3D",
    "H: help": "H: ajuda",
})

TRANSLATIONS.update({
    "Lightmaps on or off (Requiem levels)": (
        "Mapas de luz activados o desactivados (niveles de Requiem)", "Lightmap attivate o disattivate (livelli di Requiem)", "Lightmaps activées ou désactivées (niveaux de Requiem)", "Lightmaps ein oder aus (Requiem-Level)", "Mapy światła włączone lub wyłączone (poziomy Requiem)", "Карты освещения вкл. или выкл. (уровни Requiem)",
        "开启或关闭光照贴图（Requiem 关卡）", "ライトマップのオン／オフ（Requiem のレベル）", "تشغيل خرائط الإضاءة أو إيقافها (مراحل Requiem)", "הפעלה או כיבוי של מפות התאורה (שלבים של Requiem)"),
})
PT_BR.update({
    "Lightmaps on or off (Requiem levels)": "Mapas de luz ligados ou desligados (fases do Requiem)",
})

# the buttons of the 3D viewers (Ui/ViewNavigation: ViewToolBar) and the brightness of the lightmaps
TRANSLATIONS.update({
    "Wireframe": ("Malla de alambre", "Wireframe", "Fil de fer", "Drahtgitter", "Siatka", "Каркас", "线框", "ワイヤーフレーム", "الإطار السلكي", "מסגרת תיל"),
    "Back faces hidden": ("Caras traseras ocultas", "Facce posteriori nascoste", "Faces arrière masquées", "Rückseiten ausgeblendet", "Tylne ściany ukryte", "Задние грани скрыты",
        "隐藏背面", "裏面を隠す", "الأوجه الخلفية مخفية", "פאות אחוריות מוסתרות"),
    "Lightmaps (Requiem levels)": ("Mapas de luz (niveles de Requiem)", "Lightmap (livelli di Requiem)", "Lightmaps (niveaux de Requiem)", "Lightmaps (Requiem-Level)", "Mapy światła (poziomy Requiem)", "Карты освещения (уровни Requiem)",
        "光照贴图（Requiem 关卡）", "ライトマップ（Requiem のレベル）", "خرائط الإضاءة (مراحل Requiem)", "מפות תאורה (שלבים של Requiem)"),
    "Darker lightmaps": ("Mapas de luz más oscuros", "Lightmap più scure", "Lightmaps plus sombres", "Dunklere Lightmaps", "Ciemniejsze mapy światła", "Темнее карты освещения",
        "光照贴图更暗", "ライトマップを暗く", "خرائط إضاءة أغمق", "מפות תאורה כהות יותר"),
    "Brighter lightmaps": ("Mapas de luz más claros", "Lightmap più chiare", "Lightmaps plus claires", "Hellere Lightmaps", "Jaśniejsze mapy światła", "Светлее карты освещения",
        "光照贴图更亮", "ライトマップを明るく", "خرائط إضاءة أفتح", "מפות תאורה בהירות יותר"),
    "Lightmaps darker or brighter (Requiem levels)": (
        "Mapas de luz más oscuros o más claros (niveles de Requiem)", "Lightmap più scure o più chiare (livelli di Requiem)", "Lightmaps plus sombres ou plus claires (niveaux de Requiem)", "Lightmaps dunkler oder heller (Requiem-Level)", "Mapy światła ciemniejsze lub jaśniejsze (poziomy Requiem)", "Карты освещения темнее или светлее (уровни Requiem)",
        "光照贴图更暗或更亮（Requiem 关卡）", "ライトマップを暗く／明るく（Requiem のレベル）", "خرائط إضاءة أغمق أو أفتح (مراحل Requiem)", "מפות תאורה כהות או בהירות יותר (שלבים של Requiem)"),
    "left button drag": ("arrastrar con el botón izquierdo", "trascinare con il tasto sinistro", "glisser avec le bouton gauche", "mit der linken Taste ziehen", "przeciąganie lewym przyciskiem", "перетаскивание левой кнопкой",
        "按住左键拖动", "左ボタンでドラッグ", "السحب بالزر الأيسر", "גרירה בלחצן השמאלי"),
    "Turn left": ("Girar a la izquierda", "Ruotare a sinistra", "Tourner à gauche", "Nach links drehen", "Obróć w lewo", "Повернуть влево", "向左旋转", "左に回す", "تدوير لليسار", "סיבוב שמאלה"),
    "Turn right": ("Girar a la derecha", "Ruotare a destra", "Tourner à droite", "Nach rechts drehen", "Obróć w prawo", "Повернуть вправо", "向右旋转", "右に回す", "تدوير لليمين", "סיבוב ימינה"),
    "Tilt up": ("Inclinar hacia arriba", "Inclinare verso l'alto", "Incliner vers le haut", "Nach oben kippen", "Pochyl w górę", "Наклонить вверх", "向上倾斜", "上に傾ける", "إمالة لأعلى", "הטיה למעלה"),
    "Tilt down": ("Inclinar hacia abajo", "Inclinare verso il basso", "Incliner vers le bas", "Nach unten kippen", "Pochyl w dół", "Наклонить вниз", "向下倾斜", "下に傾ける", "إمالة لأسفل", "הטיה למטה"),
    "Move left": ("Mover a la izquierda", "Spostare a sinistra", "Déplacer à gauche", "Nach links verschieben", "Przesuń w lewo", "Сдвинуть влево", "向左移动", "左へ動かす", "تحريك لليسار", "הזזה שמאלה"),
    "Left arrow": ("Flecha izquierda", "Freccia sinistra", "Flèche gauche", "Pfeil nach links", "Strzałka w lewo", "Стрелка влево", "左方向键", "左矢印", "السهم الأيسر", "חץ שמאלה"),
    "Move right": ("Mover a la derecha", "Spostare a destra", "Déplacer à droite", "Nach rechts verschieben", "Przesuń w prawo", "Сдвинуть вправо", "向右移动", "右へ動かす", "تحريك لليمين", "הזזה ימינה"),
    "Right arrow": ("Flecha derecha", "Freccia destra", "Flèche droite", "Pfeil nach rechts", "Strzałka w prawo", "Стрелка вправо", "右方向键", "右矢印", "السهم الأيمن", "חץ ימינה"),
    "Move up": ("Mover hacia arriba", "Spostare in alto", "Déplacer vers le haut", "Nach oben verschieben", "Przesuń w górę", "Сдвинуть вверх", "向上移动", "上へ動かす", "تحريك لأعلى", "הזזה למעלה"),
    "Up arrow": ("Flecha arriba", "Freccia su", "Flèche haut", "Pfeil nach oben", "Strzałka w górę", "Стрелка вверх", "上方向键", "上矢印", "السهم لأعلى", "חץ למעלה"),
    "Move down": ("Mover hacia abajo", "Spostare in basso", "Déplacer vers le bas", "Nach unten verschieben", "Przesuń w dół", "Сдвинуть вниз", "向下移动", "下へ動かす", "تحريك لأسفل", "הזזה למטה"),
    "Down arrow": ("Flecha abajo", "Freccia giù", "Flèche bas", "Pfeil nach unten", "Strzałka w dół", "Стрелка вниз", "下方向键", "下矢印", "السهم لأسفل", "חץ למטה"),
    "Zoom in": ("Acercar", "Ingrandire", "Zoomer", "Vergrößern", "Przybliż", "Приблизить", "放大", "拡大", "تكبير", "התקרבות"),
    "Zoom out": ("Alejar", "Ridurre", "Dézoomer", "Verkleinern", "Oddal", "Отдалить", "缩小", "縮小", "تصغير", "התרחקות"),
    "Frame the model again": ("Encuadrar de nuevo el modelo", "Inquadrare di nuovo il modello", "Recadrer le modèle", "Modell wieder einpassen", "Ponownie wykadruj model", "Снова вписать модель",
        "重新框住模型", "モデルを再び収める", "إعادة تأطير النموذج", "מסגור מחדש של המודל"),
    "Help of the controls": ("Ayuda de los controles", "Guida dei comandi", "Aide des commandes", "Hilfe zur Steuerung", "Pomoc dotycząca sterowania", "Справка по управлению",
        "操作帮助", "操作のヘルプ", "مساعدة عناصر التحكم", "עזרה לפקדים"),
})
PT_BR.update({
    "Wireframe": "Aramado",
    "Back faces hidden": "Faces de trás ocultas",
    "Lightmaps (Requiem levels)": "Mapas de luz (fases do Requiem)",
    "Darker lightmaps": "Mapas de luz mais escuros",
    "Brighter lightmaps": "Mapas de luz mais claros",
    "Lightmaps darker or brighter (Requiem levels)": "Mapas de luz mais escuros ou mais claros (fases do Requiem)",
    "left button drag": "arrastar com o botão esquerdo",
    "Turn left": "Girar para a esquerda",
    "Turn right": "Girar para a direita",
    "Tilt up": "Inclinar para cima",
    "Tilt down": "Inclinar para baixo",
    "Move left": "Mover para a esquerda",
    "Left arrow": "Seta para a esquerda",
    "Move right": "Mover para a direita",
    "Right arrow": "Seta para a direita",
    "Move up": "Mover para cima",
    "Up arrow": "Seta para cima",
    "Move down": "Mover para baixo",
    "Down arrow": "Seta para baixo",
    "Zoom in": "Aproximar",
    "Zoom out": "Afastar",
    "Frame the model again": "Enquadrar o modelo de novo",
    "Help of the controls": "Ajuda dos controles",
})
