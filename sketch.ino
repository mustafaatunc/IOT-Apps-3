#include <WiFi.h> // ESP32'nin WiFi ağlarına bağlanabilmesi için gerekli olan temel kütüphaneyi dahil ediyoruz.
#include <PubSubClient.h> // Node-RED ile MQTT protokolü üzerinden haberleşebilmek için kütüphaneyi projeye ekliyoruz.

// Pin Tanımlamaları
#define TRIG_PIN 25 // Ultrasonik sensörün tetikleme (TRIG) pinini ESP32'nin 25 numaralı pinine tanımlıyoruz.
#define ECHO_PIN 34 // Ultrasonik sensörün yankı (ECHO) pinini ESP32'nin 34 numaralı pinine tanımlıyoruz.
#define PIR_PIN 19  // PIR hareket sensörünün veri (OUT) pinini ESP32'nin 19 numaralı pinine tanımlıyoruz.

// RGB LED Pinleri 
#define LED_R_PIN 5  // RGB LED'in Kırmızı (Red) bacağını 5 numaralı pine tanımlıyoruz.
#define LED_G_PIN 17 // RGB LED'in Yeşil (Green) bacağını 17 numaralı pine tanımlıyoruz.
#define LED_B_PIN 16 // RGB LED'in Mavi (Blue) bacağını 16 numaralı pine tanımlıyoruz.

// WiFi Ayarları
const char* ssid = "Wokwi-GUEST"; // Wokwi simülasyon ortamının standart ve şifresiz sanal ağ adını tanımlıyoruz.
const char* password = ""; // Ağın şifresi olmadığı için boş bir string (metin) olarak bırakıyoruz.

// MQTT Broker ve Topic Ayarları
const char* mqtt_server = "test.mosquitto.org"; // Ücretsiz ve herkese açık MQTT sunucusunu tanımlıyoruz.
const char* pub_topic = "19010011108/mesafe"; // Sensör mesafe verilerini yayınlayacağımız kanal.
const char* sub_topic = "19010011108/kontrol"; // Node-RED arayüzündeki butondan komutları dinleyeceğimiz kanal.

WiFiClient espClient; // ESP32'nin WiFi bağlantı altyapısını kuracak istemci nesnesini oluşturuyoruz.
PubSubClient client(espClient); // MQTT işlemlerini bu WiFi istemcisi üzerinden yürütecek ana nesneyi tanımlıyoruz.

// Sistem Durum Değişkenleri
volatile bool alarm_izin = true; // Node-RED üzerinden butona basıldığında alarmın açılıp kapanma durumunu kontrol eden bayrak değişkeni.
volatile bool sartlar_saglandi = false; // Hareket algılanması ve mesafenin 0-200cm arasında olması durumunun aynı anda sağlanıp sağlanmadığını tutar.
volatile bool led_durumu = false; // Yanıp sönme animasyonu için LED'in o anki lojik durumunu hafızada tutar.
hw_timer_t * timer = NULL; // ESP32'nin donanımsal zamanlayıcısına erişmek için bellekte bir işaretçi tanımlıyoruz.
long son_olcum_zaman = 0; // İşlemciyi sürekli ölçümle yormamak adına son ölçümün yapıldığı milisaniyeyi kaydediyoruz.

// Timer Kesmesi (Interrupt) Fonksiyonu - LED'in 500ms aralıklarla yanıp sönmesi için
void IRAM_ATTR onTimer() { // Bu fonksiyonun hızlı tepki vermesi için doğrudan RAM üzerinde çalışmasını sağlıyoruz.
  if (alarm_izin && sartlar_saglandi) { // Eğer Node-RED'den sisteme izin verilmişse VE mesafe/hareket şartları oluşmuşsa:
    led_durumu = !led_durumu; // LED'in mevcut durumunun tersini alıyoruz (yanıksa söndürecek, sönükse yakacak sinyal hazırlığı).
    // Görseldeki LED "Ortak Anot" bağlı olduğu için LOW sinyalinde yanar, HIGH sinyalinde söner.
    digitalWrite(LED_R_PIN, led_durumu ? LOW : HIGH); // Hazırlanan yeni lojik durumu LED'in kırmızı pinine gönderiyoruz.
  } else { // Eğer hareket veya mesafe şartları sağlanmıyorsa veya alarm Node-RED'den kapatılmışsa:
    digitalWrite(LED_R_PIN, HIGH); // LED'in kırmızı bacağına HIGH (5V) göndererek tamamen sönük durumda kalmasını garanti ediyoruz.
    led_durumu = false; // Bir sonraki tetiklenmede sıranın bozulmaması için değişkeni kapalı duruma sıfırlıyoruz.
  } 
}

void setup_wifi() { // ESP32'nin WiFi modülünü Wokwi ağına bağlayan yardımcı fonksiyonumuz.
  delay(10); // İşlemlere başlamadan önce donanımın elektriksel olarak stabil hale gelmesi için çok kısa bir süre bekliyoruz.
  Serial.println(); // Seri port (konsol) ekranında çıktıların birbirine yapışmaması için boş bir satır atlıyoruz.
  Serial.print("Baglaniliyor: "); // Kullanıcıyı bilgilendirmek amacıyla ağ bağlantı sürecinin başladığını ekrana yazdırıyoruz.
  Serial.println(ssid); // Kodun başında tanımladığımız bağlanılacak ağın adını (SSID) ekrana yazdırıyoruz.
  WiFi.begin(ssid, password); // WiFi modülünü belirlediğimiz ağ adı ve boş şifre ile başlatarak bağlanma komutunu veriyoruz.
  while (WiFi.status() != WL_CONNECTED) { // WiFi modülünün durumu 'Bağlandı' olana kadar bekleyecek bir döngü kuruyoruz.
    delay(500); // Sistemin kilitlenmesini engellemek için her bağlantı denemesi arasında yarım saniye (500 ms) bekliyoruz.
    Serial.print("."); // Bağlantı sürecinin devam ettiğini görsel olarak belli etmek için yan yana noktalar koyuyoruz.
  } // WiFi bağlanma döngüsünün (while) sonu.
  Serial.println(""); // Bağlantı döngüsünden çıkıldığında (başarıyla bağlandığında) bir alt satıra geçiyoruz.
  Serial.println("WiFi baglantisi basarili!"); // Seri port ekranında ağa başarıyla bağlanıldığını belirtiyoruz.
}

void callback(char* topic, byte* payload, unsigned int length) { // MQTT sunucusundan cihaza bir mesaj geldiğinde otomatik tetiklenen fonksiyon.
  String mesaj = ""; // Gelen bayt formatındaki veriyi, işlem yapabileceğimiz düz metne (String) çevirmek için boş bir değişken tanımlıyoruz.
  for (int i = 0; i < length; i++) { // Gelen mesajın uzunluğu kadar tekrar edecek bir döngü başlatıyoruz.
    mesaj += (char)payload[i]; // Gelen her bir baytı standart karakterlere dönüştürüp, mesaj isimli string değişkenimize ekliyoruz.
  }
  if (mesaj == "KAPAT") { // Eğer Node-RED arayüzündeki butondan cihazı durdurmak için "KAPAT" metni gönderilmişse:
    alarm_izin = false; // Timer kesmesi içindeki şartı bozarak LED animasyonunu durdurmak üzere alarm iznini iptal ediyoruz.
    Serial.println("Alarm Node-RED tarafindan KAPATILDI."); // Cihazın durdurulduğu bilgisini konsol ekranına bilgi amaçlı yazdırıyoruz.
  } else if (mesaj == "AC") { // Eğer Node-RED arayüzündeki butondan cihazı tekrar başlatmak için "AC" metni gönderilmişse:
    alarm_izin = true; // Timer kesmesinin tekrar çalışabilmesi için sisteme alarm iznini yeniden veriyoruz.
    Serial.println("Alarm Node-RED tarafindan ACILDI."); // Cihazın tekrar aktif edildiği bilgisini konsol ekranına bilgi amaçlı yazdırıyoruz.
  } 
}

void reconnect() { // MQTT sunucusu ile bağlantı herhangi bir sebeple kesilirse tekrar bağlanmayı deneyecek fonksiyon.
  while (!client.connected()) { // İstemci nesnesi MQTT sunucusuna tamamen bağlanana kadar çalışacak bir döngü başlatıyoruz.
    Serial.print("MQTT broker'a baglaniliyor..."); // Tekrar bağlantı girişiminin başladığını seri port ekranına bildiriyoruz.
    String clientId = "ESP32Client-"; // Birden fazla ESP32 olması durumuna karşı benzersiz bir ID oluşturmak için ön ek yazıyoruz.
    clientId += String(random(0xffff), HEX); // Sunucuda çakışma olmaması adına bu ön ekin sonuna rastgele oluşturulmuş bir hex sayı ekliyoruz.
    if (client.connect(clientId.c_str())) { // Oluşturduğumuz bu benzersiz istemci kimliği ile MQTT sunucusuna bağlanma isteği atıyoruz.
      Serial.println(" Baglandi!"); // Eğer connect() komutu başarılı dönerse ekrana bağlandı bilgisini yazdırıyoruz.
      client.subscribe(sub_topic); // Bağlantı kurulur kurulmaz Node-RED butonunu dinleyebilmek için kontrol kanalına (topic) abone oluyoruz.
    } else { // Eğer MQTT sunucusu bağlanma isteğini reddederse veya zaman aşımına uğrarsa:
      Serial.print("Hata durumu: "); // Hata mesajının başlığını ekrana yazdırıyoruz.
      Serial.print(client.state()); // MQTT kütüphanesinden dönen ve sorunun ne olduğunu belirten hata durum kodunu basıyoruz.
      Serial.println(" 5 saniye sonra tekrar denenecek."); // Ağ trafiğini kitlememek için bekleme yapılacağını haber veriyoruz.
      delay(5000); // Sistemi yeniden bağlantı döngüsüne sokmadan önce 5 saniye (5000 ms) dinlendiriyoruz.
    } 
  } 
}

long mesafeOlc() { // Ultrasonik sensörün ürettiği ses dalgaları aracılığıyla engelin mesafesini santimetre (cm) cinsinden hesaplayan fonksiyon.
  digitalWrite(TRIG_PIN, LOW); // Yeni bir ölçüme pürüzsüz başlamak için tetikleme pinini düşük voltaja (LOW) çekip temizliyoruz.
  delayMicroseconds(2); // Pinin tamamen kapanıp lojik '0' seviyesine oturabilmesi için 2 mikrosaniye donanıma izin veriyoruz.
  digitalWrite(TRIG_PIN, HIGH); // Sensörün ultrasonik bir dalga paketi göndermesi için tetikleme pinini yüksek (HIGH) yapıyoruz.
  delayMicroseconds(10); // Sensörün datasheet dokümanına uygun olarak tetikleme sinyalini tam 10 mikrosaniye boyunca havada tutuyoruz.
  digitalWrite(TRIG_PIN, LOW); // Dalga paketi gönderildikten sonra pini kapatarak sensörün yankı dinleme moduna geçmesini sağlıyoruz.
  long sure = pulseIn(ECHO_PIN, HIGH); // Gönderilen dalganın engele çarpıp geri dönmesiyle oluşan sinyalin süresini mikrosaniye olarak ölçüyoruz.
  long mesafe = sure * 0.034 / 2; // Havada sesin hızını (0.034 cm/us) kullanarak süreyi çarpıyor ve sinyal gidiş-dönüş yaptığı için ikiye bölüyoruz.
  return mesafe; // Hesaplama sonucu elde ettiğimiz saf santimetre değerini bu fonksiyonu çağıran ana kod bloğuna iletiyoruz.
}

void setup() { // ESP32 donanımına ilk güç verildiğinde sadece bir kez çalışan temel hazırlık ve konfigürasyon fonksiyonumuz.
  Serial.begin(115200); // Sensör okumalarını ve cihazın bağlantı durumlarını görebilmek için seri haberleşmeyi 115200 hızında başlatıyoruz.
  
  pinMode(PIR_PIN, INPUT); // PIR hareket sensöründen ESP32'ye veri geleceği için bu pini GİRİŞ (INPUT) olarak ayarlıyoruz.
  pinMode(TRIG_PIN, OUTPUT); // Ultrasonik sensöre dalga üretmesini emredeceğimiz için tetikleme pinini ÇIKIŞ (OUTPUT) yapıyoruz.
  pinMode(ECHO_PIN, INPUT); // Ultrasonik sensörden seken dalganın bilgisini okuyacağımız için yankı pinini GİRİŞ (INPUT) yapıyoruz.
  
  pinMode(LED_R_PIN, OUTPUT); // RGB LED'in Kırmızı ışık pinini enerji göndermek üzere ÇIKIŞ (OUTPUT) olarak konfigüre ediyoruz.
  pinMode(LED_G_PIN, OUTPUT); // RGB LED'in Yeşil ışık pinini enerji göndermek üzere ÇIKIŞ (OUTPUT) olarak konfigüre ediyoruz.
  pinMode(LED_B_PIN, OUTPUT); // RGB LED'in Mavi ışık pinini enerji göndermek üzere ÇIKIŞ (OUTPUT) olarak konfigüre ediyoruz.

  // Devredeki LED Ortak Anot (Common Anode) mantığı ile bağlandığı için enerjiyi kesmek amacıyla başlangıçta pinleri HIGH (5V) yapıyoruz.
  digitalWrite(LED_R_PIN, HIGH); // Başlangıçta kırmızı ışığın sönük kalmasını garanti ediyoruz.
  digitalWrite(LED_G_PIN, HIGH); // Başlangıçta yeşil ışığın sönük kalmasını garanti ediyoruz.
  digitalWrite(LED_B_PIN, HIGH); // Başlangıçta mavi ışığın sönük kalmasını garanti ediyoruz.

  setup_wifi(); // Yukarıda yazdığımız WiFi ağlarına bağlanma rutinini cihaz ilk açıldığında çalıştırıyoruz.
  client.setServer(mqtt_server, 1883); // MQTT kütüphanesine bağlanacağı sunucunun web adresini ve standart port numarası olan 1883'ü veriyoruz.
  client.setCallback(callback); // Node-RED üzerinden cihaza veri gelmesi durumunda 'callback' fonksiyonunun çalışmasını emrediyoruz.

  // Timer Interrupt (Zamanlayıcı Kesmesi) Konfigürasyonu
  timer = timerBegin(1000000); // ESP32'nin donanımsal zamanlayıcısını 1MHz frekans (her mikrosaniyede 1 tık) ile başlatıyoruz.
  timerAttachInterrupt(timer, &onTimer); // Zamanlayıcı dolduğunda kodun başındaki 'onTimer' kesme fonksiyonunun tetiklenmesini sağlıyoruz.
  timerAlarm(timer, 500000, true, 0); // Kesmenin 500.000 mikrosaniyede (500ms) bir çalışmasını ve sürekli tekrar etmesini (true) söylüyoruz.
}

void loop() { // ESP32 açık olduğu sürece sonsuz bir döngü halinde sürekli olarak tekrar edecek olan ana işlem mekanizması.
  if (!client.connected()) { // Eğer o anki dönüş adımında MQTT sunucusu ile olan bağlantı herhangi bir sebeple kopmuş durumda ise:
    reconnect(); // Bağlantıyı tekrar kurmak için onarım ve yeniden bağlanma fonksiyonunu çağırıyoruz.
  } 
  
  client.loop(); // MQTT istemcisinin arka planda mesaj okuyabilmesi ve sunucu ile iletişimi taze tutması için çalıştırıyoruz.

  long suan = millis(); // Cihaza ilk elektrik verildiğinden beri geçen zamanı milisaniye cinsinden alıp 'suan' isimli bir değişkende tutuyoruz.
  if (suan - son_olcum_zaman > 1000) { // Sensörleri her an okuyup sistemi boğmamak için, son ölçümün üzerinden 1 saniye (1000ms) geçmiş mi diye bakıyoruz.
    son_olcum_zaman = suan; // Bir sonraki 1 saniyelik hesabın doğru çalışması için ölçüm zamanı değişkenini mevcut saat ile güncelliyoruz.

    int hareket = digitalRead(PIR_PIN); // PIR sensöründen gelen ham durumu okuyoruz (Eğer hareket varsa değer 'HIGH' (1) olacaktır).
    long mesafe = mesafeOlc(); // Kendi hesaplama fonksiyonumuzu kullanarak engele olan mesafeyi cm cinsinden alıyoruz.

    Serial.print("Hareket: "); // Geliştirici ekranına verilerin anlamlı görünmesi için hareket bilgisinin başlığını yazdırıyoruz.
    Serial.print(hareket); // PIR sensöründen aldığımız 0 veya 1 değerini seri port ekranına basıyoruz.
    Serial.print(" | Mesafe: "); // Çıktıların birbirine girmemesi ve estetik görünmesi için bir ayraç metni yazdırıyoruz.
    Serial.println(mesafe); // Ultrasonik sensörün hesapladığı mesafeyi cm olarak ekrana yazıp satırı alta geçiriyoruz.

    // Mesafeyi sürekli olarak Node-RED'e gönderiyoruz (Renkler çalışacak)
    String mesafeStr = String(mesafe); // MQTT sunucusuna veri yollayabilmek için sayısal olan mesafe değerini metin (String) formatına dönüştürüyoruz.
    client.publish(pub_topic, mesafeStr.c_str()); // Dönüştürdüğümüz bu mesafe metnini, Node-RED üzerinde görünmesi için ilgili MQTT kanalına iletiyoruz.

    // Sadece tehlike anında (0-200cm ve hareket varken) LED yanıp sönecek.
    if (hareket == HIGH && mesafe >= 0 && mesafe <= 200) {  // Senaryo kontrolü: Hareket var mı VE mesafe 0 ile 200 cm aralığında mı?
      sartlar_saglandi = true; // Timer ISR kesmesinin LED'i yakıp söndürmeye başlayabilmesi için global şart sağlandı bayrağını DOĞRU yapıyoruz.
    } else { // Eğer ortamda hareket algılanmıyorsa veya engel mesafesi 200 cm'den daha uzaktaysa:
      sartlar_saglandi = false; // Timer ISR kesmesinin LED animasyonunu derhal durdurması için global şart sağlandı bayrağını YANLIŞ yapıyoruz.
    } 
  } 
} 