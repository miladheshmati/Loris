import * as R from 'ramda';
import {connect} from 'react-redux';
import {
  startDragInterval,
  continueDragInterval,
  endDragInterval,
} from '../store/logic/dragBounds';
import {setInterval} from '../store/state/bounds';
import {updateFilteredEpochs} from '../store/logic/filterEpochs';
import {Slider, Rail, Handles, Ticks} from 'react-compound-slider';
import {Handle, Tick} from './components';
import React, {useState, FunctionComponent} from 'react';
import {RootState} from '../store';

type CProps = {
  viewerHeight?: number,
  domain: [number, number],
  interval: [number, number],
  setInterval: (_: [number, number]) => void,
  dragStart: (_: [number, number]) => void,
  dragContinue: (_: [number, number]) => void,
  dragEnd: (_: [number, number]) => void,
  updateFilteredEpochs: (_: void) => void,
};

/**
 *
 * @param root0
 * @param root0.viewerHeight
 * @param root0.domain
 * @param root0.interval
 * @param root0.setInterval
 * @param root0.dragStart
 * @param root0.dragContinue
 * @param root0.dragEnd
 * @param root0.updateFilteredEpochs
 */
const IntervalSelect: FunctionComponent<CProps> = ({
  viewerHeight,
  domain,
  interval,
  setInterval,
  dragStart,
  dragContinue,
  dragEnd,
  updateFilteredEpochs,
}) => {
  const [isDragging, setIsDragging] = useState(false);

  const sliderStyle = {
    position: 'relative',
  };

  const railStyle = {
    position: 'absolute',
    width: '100%',
    height: 10,
    marginTop: -9,
    borderBottom: '1px solid #000',
    cursor: 'pointer',
  };

  return (
    <div className='row'>
      <div
        style={{
          display: 'flex',
          flexDirection: 'column',
          justifyContent: 'center',
          alignItems: 'center',
        }}
      >
        <h5
          className='col-xs-offset-1 col-xs-11'
          style={{
            color: '#064785',
            fontWeight: 'bold',
            paddingLeft: '15px',
            marginBottom: '15px',
            textAlign: 'center',
          }}
        >
          Timeline Range View
        </h5>
        <div
          className='col-xs-offset-1 col-xs-11'
          style={{
            marginBottom: '20px',
            display: 'flex',
            justifyContent: 'center',
          }}
        >
          <div className='btn-group'>
            <input
              type='button'
              className='btn btn-primary btn-xs'
              onClick={() => {
                if (interval[0] !== domain[0]) {
                  setInterval([interval[0] - 50, interval[1] - 50]);
                } else {
                  setInterval([interval[0], interval[1] - 50]);
                }
              }}
              value='<<'
            />
            <input
              type='button'
              className='btn btn-primary btn-xs'
              onClick={() => {
                if (interval[0] !== domain[0]) {
                  setInterval([interval[0] - 1, interval[1] - 1]);
                } else {
                  setInterval([interval[0], interval[1] - 1]);
                }
              }}
              value='<'
            />
            <input
              type='button'
              className='btn btn-primary btn-xs'
              onClick={() => {
                if (interval[1] !== domain[1]) {
                  setInterval([interval[0] + 1, interval[1] + 1]);
                } else {
                  setInterval([interval[0] + 1, interval[1]]);
                }
              }}
              value='>'
            />
            <input
              type='button'
              className='btn btn-primary btn-xs'
              onClick={() => {
                if (interval[1] !== domain[1]) {
                  setInterval([interval[0] + 50, interval[1] + 50]);
                } else {
                  setInterval([interval[0] + 50, interval[1]]);
                }
              }}
              value='>>'
            />
          </div>
          <input
            type='button'
            className='btn btn-primary btn-xs'
            onClick={() => {
              setInterval([domain[0], domain[1]]);
              updateFilteredEpochs();
            }}
            value='Reset'
            style={{marginLeft: '15px'}}
          />
        </div>
      </div>
      <div
        className='col-xs-offset-1 col-xs-11'
        style={{height: viewerHeight}}
      >
        <Slider
          mode={2}
          rootStyle={sliderStyle}
          domain={[domain[0], domain[1]]}
          values={[interval[0], interval[1]]}
          onUpdate={(values) => {
            if (!isDragging) {
              dragStart([values[0], values[1]]);
              setIsDragging(true);
            } else {
              dragContinue([values[0], values[1]]);
            }
          }}
          onChange={(values) => {
            dragStart([values[0], values[1]]);
            dragEnd([values[0], values[1]]);
            setIsDragging(false);
          }}
        >
          {/* @ts-ignore */}
          <Rail>
            {({getRailProps}) => (
              <div style={railStyle} {...getRailProps()} />
            )}
          </Rail>
          {/* @ts-ignore */}
          <Handles>
            {({handles, getHandleProps}) => (
              <div className="slider-handles">
                {handles.map((handle) => (
                  <Handle
                    key={handle.id}
                    handle={handle}
                    domain={domain}
                    getHandleProps={getHandleProps}
                  />
                ))}
              </div>
            )}
          </Handles>
          {/* @ts-ignore */}
          <Ticks count={10}>
            {({ticks}) => (
              <div
                className="slider-ticks"
                style={{
                  position: 'relative',
                  zIndex: 1,
                  pointerEvents: 'none',
                }}
              >
                {ticks.map((tick) => (
                  <Tick key={tick.id} tick={tick} count={ticks.length} />
                ))}
              </div>
            )}
          </Ticks>
        </Slider>

        <div
          style={{
            fontSize: 10,
            top: '-25px',
            right: '15px',
            position: 'absolute',
          }}
        >
          Time (s)
        </div>
      </div>
    </div>
  );
};

IntervalSelect.defaultProps = {
  viewerHeight: 20,
  domain: [0, 1],
  interval: [0.25, 0.75],
};

export default connect(
  (state: RootState) => ({
    domain: state.bounds.domain,
    interval: state.bounds.interval,
  }),
  (dispatch: (_: any) => void) => ({
    dragStart: R.compose(
      dispatch,
      startDragInterval
    ),
    dragContinue: R.compose(
      dispatch,
      continueDragInterval
    ),
    dragEnd: R.compose(
      dispatch,
      endDragInterval
    ),
    updateFilteredEpochs: R.compose(
      dispatch,
      updateFilteredEpochs
    ),
    setInterval: R.compose(
      dispatch,
      setInterval
    ),
  })
)(IntervalSelect);
