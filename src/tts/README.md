# Cloud TTS

## Add a new service checklist

* Read `service.h`.
* Implement `Service::speak`.
* Implement `Service::stop`.
* Implement `ServiceConfigWidget`, which will be embedded in `ConfigWindow`.
* Add the `Service` to `ServiceController`.
* Add the `ServiceConfigWidget` to `ConfigWindow`.
* DONE.

## Design Goals

Allow modifying / evolving any one of the services arbitrarily without incurring the need to touch another.

Avoid almost all temptation to do 💩 abstraction 💩 unless absolutely necessary.

## Code

### Config

```
(1) Service ConfigWidet --write--> (2) Service's config file --create--> (3) Live Service Object
```

* Config Serialization+Saving and Service state mutating will not happen in parallel or successively.
* (1) will neither mutate nor access (3).
* construct (3) only according to (2).

### Object management

* Service construction will be done on the service consumer side
* Service can be cast to `Service`, which only has `speak/stop` and destructor.
  * The service consumer should not care
  anything else after construction.

### Config Window

Similar to KDE's Settings module (KCM).
Every service simply provides a config widget on its own, and the config window simply loads the Widget.

### No exception

* Handle errors religiously and immediately, and report to users if user attention/action is required.

## Rational

* Services are different and testing them is hard (cloud tts usually needs an account).
* Do not assume services have any similarity other than the fact they may `speak`.
* Services on earth are limited, thus the boilerplate caused by fewer useless abstractions is also limited.
* The service consumer will use services incredibly and insanely creative in the future.
* Maintaining two code paths of object creation & mutating is a waste of time.
	* Just save config to disk, and construct objects according to what's in the disk.