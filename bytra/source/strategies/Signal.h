//
// Created by Arne Wouters on 09/08/2020.
//

#ifndef MEXTRA_SIGNAL_H
#define MEXTRA_SIGNAL_H

class Signal {
  public:
    Signal() = default;

    virtual ~Signal() = default;
};

class LongSignal : public Signal {};

class ShortSignal : public Signal {};

#endif  // MEXTRA_SIGNAL_H
