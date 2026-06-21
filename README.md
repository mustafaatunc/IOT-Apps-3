# ESP32 ve Node-RED Tabanlı Akıllı Güvenlik Sistemi

Bu proje, ESP32 geliştirme kartı kullanılarak tasarlanmış, hareket ve mesafe algılama yeteneklerine sahip IoT tabanlı bir akıllı güvenlik alarm sistemidir. Sistem donanımsal çevre birimlerini yönetirken, Node-RED platformu ve MQTT protokolü (Mosquitto Broker) üzerinden uzaktan izleme ve kontrol imkanı sunmaktadır.

## 🔗 Proje Bağlantıları

* **Wokwi Simülasyon Linki:** [Proje Simülasyonu](https://wokwi.com/projects/464723659807998977) 

## 📸 Görseller

![Devre Şeması](https://github.com/user-attachments/assets/3c73d04a-dd27-49f3-a866-60db19220ea1)
![Node-RED Dashboard](https://github.com/user-attachments/assets/3d3264ae-afea-4b17-8023-9a7f29194cca)

## 🚀 Sistem Özellikleri ve Çalışma Senaryosu

Sistem ortamdaki hareketleri ve nesne mesafelerini sürekli olarak tarayarak aşağıdaki algoritmaları işletir:

* **Asenkron Veri Aktarımı ve Dinamik Arayüz:** Ultrasonik sensörden (HC-SR04) elde edilen veriler her 1 saniyede bir MQTT broker'a aktarılır. Node-RED Dashboard üzerindeki Gauge (gösterge) paneli, bu verileri hedef mesafesine göre dinamik olarak renklendirir: `0-200 cm (Kırmızı)`, `200-300 cm (Sarı)` ve `300-400 cm (Yeşil)`.
* **Akıllı Alarm Durumu:** PIR sensörü ortamda dijital (HIGH) hareket algılarsa **VE** hedef mesafesi tehlike sınırı olan `0 ile 200 cm` aralığındaysa, sistem otomatik olarak alarm durumuna geçer.
* **Donanımsal Kesme (Timer Interrupt):** Tehlike anında, işlemciyi meşgul etmeden (non-blocking) doğrudan donanım seviyesinde çalışan bir Timer Interrupt (ISR) devreye girer. Zamanlayıcı **1 MHz (1 mikrosaniye)** taban frekansıyla kurularak RGB LED'in tam 500.000 tıkta (500 ms) bir kusursuz zamanlamayla yanıp sönmesi sağlanmıştır.
* **Uzaktan Müdahale:** Kullanıcı, Node-RED arayüzünde bulunan kontrol butonu aracılığıyla sisteme "KAPAT" veya "AC" komutları göndererek yanıp sönen alarmı istediği an durdurabilir veya yeniden aktif edebilir.

## 🛠️ Kullanılan Donanımlar ve Pin Haritası

| Bileşen | ESP32 Pini | Açıklama |
| :--- | :--- | :--- |
| **HC-SR04 (Mesafe)** | `TRIG: 25`, `ECHO: 34` | Hedef mesafesi ölçümü |
| **PIR Motion Sensörü** | `OUT: 19` | Hareket algılama (Dijital Giriş) |
| **RGB LED (Ortak Anot)**| `R: 5`, `G: 17`, `B: 16` | Görsel alarm bildirimi. *(Not: Kullanılmayan Yeşil ve Mavi bacaklar setup aşamasında HIGH konumuna çekilerek donanımsal parazitlerin önüne geçilmiştir).* |

## 🌐 Ağ ve MQTT Konfigürasyonu

Bu proje uzaktan haberleşme için `PubSubClient` kütüphanesini ve test amaçlı Mosquitto broker altyapısını kullanmaktadır. İletişim kanalları güvenlik ve çakışma kontrolü amacıyla **öğrenci numarası (19010011108)** kullanılarak kişiselleştirilmiştir.

* **WiFi SSID:** `Wokwi-GUEST` 
* **MQTT Broker:** `test.mosquitto.org` (Port: 1883) 
* **Publish Topic (Veri Gönderimi):** `19010011108/mesafe` 
* **Subscribe Topic (Veri Alma):** `19010011108/kontrol` 

## ⚙️ Kurulum ve Node-RED Çalıştırılması

1. Bu depoyu yerel bilgisayarınıza klonlayın ve kodu ESP32'ye yükleyin.
2. Bilgisayarınızda (Localhost) çalışan Node-RED sunucusunu başlatın.
3. Proje klasöründeki `19010011108_flows.json` dosyasını Node-RED arayüzüne **Import** ederek gerekli düğümleri içeri aktarın.
4. Node-RED Dashboard modüllerinin doğru yüklendiğinden (`node-red-dashboard` veya `FlowFuse`) emin olun.
5. Devreyi Wokwi üzerinde başlatın ve Node-RED arayüzü (`localhost:1880/ui`) üzerinden verileri izlemeye başlayın.
