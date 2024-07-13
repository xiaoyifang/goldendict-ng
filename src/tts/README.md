# Cloud TTS

## Add a new service checklist

* Read to `service.h`.
* Implement `Service::speak`.
* Implement `Service::stop`.
* Implement `ServiceConfigWidget`+`ServiceConfigWidget::save`.
* Add the `Service` to `ServiceController`.
* Add the `ServiceConfigWidget` to `ConfigWindow`.
* DONE.

## Design Goals

Allow modifying / evolving any one of the services arbitrarily without incurring the need to touch another.

(Conversely, old services won't be obstacles of modifying currently maintained services.)

Avoid almost all temptation to do 💩 abstraction 💩 unless absolutely necessary.

## Code

### Config

```
(1) Service ConfigWidet --write--> (2) Service's config file --create--> (3) Live Service Object
```

* Config file serialization and Service state mutating will not happen in parallel or successively.
* Service construct only based on config file. One single deterministic routine.

(1) will neither mutate nor access (3).

construct (3) only according to (2).

(1) only write to (2).

### Object management

* Service construction will be done on the service consumer side to avoid sharing constructor.
* Service can be cast to `Service`, which only has `speak/stop` and destructor. The service consumer should not care
  anything else after construction.
* Services don't share global config, don't share config mechanisms

### Config Window

Similar to KDE's Settings module (KCM).
Every service simply provides a config widget on its own, and the config window simply loads the Widget.

## Rational

* Services are different.
* Testing services are hard (cloud tts usually needs an account).
* Abstracting seemingly similar but different things will eventually blunder.
* Do not assume services have any similarity other than the fact they may `speak`.
* Construction materials needed by services are different.
* Services on earth are limited, thus the boilerplate caused by fewer useless abstractions is also limited
* The service consumer will use services incredibly and insanely creative in the future.
* Maintaining two code paths of object creation & mutating is a waste of time.
