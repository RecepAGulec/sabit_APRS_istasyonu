# sabit_APRS_istasyonu
Belli bir konumdan meteorolojik veri aktarımı için
Belli bir konumdan bâzı meteorolojik verilerin ve gerekiyorsa başka verilerin en kolay şekilde aktarılması için APRS sisteminden faydalanılabilir. Bu amaçla
bir APRS sistemi arayışına girdim. Bulduğum pek çok proje konum ve hız bilgilerini iletmek üzere tasarlanmıştı. İstediğim gibi çalışan uygulamalar da ticarî
idi.
Çeşitli projeleri inceledikten sonra billygr'nin https://github.com/billygr/aprs-tracker-without-gps-module adresindeki uygulamayı incelemeye başladım.
Bu uygulama, GPS'e ihtiyaç göstermeden, elle pozisyon bilgisi girmeye izin veriyordu. Bu yazılımı çekirden olarak alarak bâzı değişiklik ve ilaveler yaptım:
1) Standart koordinat girişi yerine NMEA formatında koordinat girerek koddan kısmî tasarruf sağladım.
2) Jerome LOYET'in DRA818 kütüphânesi ( https://github.com/fatpat/arduino-dra818 ) kullanılarak bir SA818 (DRA818) VHF alıcı-verici modülüne arduino ile
kumanda ilave edilmiştir.
3) Bâzı verilerin APRS paketinin commant kısmında gönderilebilmesi sağlanmıştır. Bu verileri comment satırına uygularken 
https://forum.arduino.cc/t/including-variables-in-text-with-aprs/597188 adresindeki bilgilerden faydalanılmıştır.
4) BMP280 algılayıcısı kullanılarak sıcaklık ve basınç verileri elde edilmiş ve bu veriler comment satırında gösterilir hâle getirilmiştir.
5) Devre, tek bir Li-Ion pille beslendiğinden, pil gerilimi keza comment satırında gösterilir hâle getirilmiştir.
Devre şu an test aşamasındadır. Daha sonra devreye bir güneş paneli ve bir doldurma devresi ilâve edilecektir.
