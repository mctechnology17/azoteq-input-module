> [!CAUTION]
> THIS MODULE HAS NOT BEEN TESTED, SO I DO NOT RECOMMEND USING IT, IF YOU
> INSTALL IT YOU DO IT AT YOUR OWN RISK !!!!

Driver for the azoteq trackpad with ZMK software
# Driver for the azoteq trackpad

<!-- mc-follow-top:start -->
<div align="center">

  <a href="https://youtube.com/@mctechnology17"><img align="center" alt="MC Technology | YouTube" width="22px" src="https://github.com/mctechnology17/mctechnology17/blob/main/src/youtube.png" /></a>
  <a href="https://www.instagram.com/mctechnology17/"><img align="center" alt="MC Technology | Instagram" width="22px" src="https://github.com/mctechnology17/mctechnology17/blob/main/src/instagram.png" /></a>
  <a href="https://www.facebook.com/mctechnology17/"><img align="center" alt="MC Technology | Facebook" width="22px" src="https://github.com/mctechnology17/mctechnology17/blob/main/src/facebook.png" /></a>
  <a href="https://www.tiktok.com/@mctechnology17"><img align="center" alt="MC Technology | TikTok" width="22px" src="https://github.com/mctechnology17/mctechnology17/blob/main/src/tiktok.png" /></a>

</div>
<!-- mc-follow-top:end -->

# config example

```conf
CONFIG_INPUT=y
CONFIG_INPUT_AZOTEQ_IQS5XX=y
# CONFIG_ZMK_INPUT_AZOTEQ_IQS5XX_IDLE_SLEEPER=y
# CONFIG_INPUT_AZOTEQ_IQS5XX_INIT_PRIORITY=60
```

### Example Devicetree Overlay
```dts
&i2c1 {
    iqs5xx: iqs5xx@74 {
        compatible = "azoteq,iqs5xx";
        reg = <0x74>;
	// same pin like niceview
        dr-gpios = <&gpio0 1 GPIO_ACTIVE_LOW>;
        x-invert = <false>;
        y-invert = <false>;
        no-taps = <false>;
    };
};
```

5 pins are needed to configure the azoteq trackpad:

1. **Power**:
   - **3V** on the nice!nano -> **VDD** on the IQS5xx.
   - **G** (Ground) on the nice!nano -> **GND** on the IQS5xx.

2. **I2C Signals**:
   - **SDA** (Feather pin labeled "SDA") on the nice!nano -> **SDA** on the
     IQS5xx.
   - **SCL** (Feather pin labeled "SCL") on the nice!nano -> **SCL** on the
     IQS5xx.

3. **Data Ready / Interrupt Pin**:
   - The IQS5xx "DR" or "RDY" pin -> **Any available GPIO** on the nice!nano.
   - For example, you can use **D2**, **D3**, or **A0**—whichever is free in
     your design. In devicetree, you’ll reference this pin under `dr-gpios`.

<!-- mc-follow:start -->
# Support this project

This is a personal project that I build in my free time. If it is useful to
you, the best way to help is to follow and subscribe:

[YouTube](https://youtube.com/@mctechnology17) · [Instagram](https://www.instagram.com/mctechnology17/) · [Facebook](https://www.facebook.com/mctechnology17/) · [TikTok](https://www.tiktok.com/@mctechnology17)

I make videos about split keyboards, ZMK and open source (in Spanish, long
videos with English subtitles). Every follow and every subscription helps me a
lot and keeps me motivated to bring more to the open source community.

Thank you for every report and every issue: they make these projects better.
<!-- mc-follow:end -->

[//]: # ( vim: set fdm=marker: )
