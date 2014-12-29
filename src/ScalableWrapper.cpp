#include "ScalableWrapper.h"

#include <QGraphicsProxyWidget>
#include <QMenu>
#include <QScrollBar>
#include <QTextEdit>


ScalableWrapper::ScalableWrapper(QTextEdit* _editor, QWidget* _parent) :
	QGraphicsView(_parent),
	m_scene(new QGraphicsScene),
	m_editor(_editor),
	m_zoomRange(1)
{
	//
	// Всегда показываем полосы прокрутки
	//
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	//
	// Предварительная настройка редактора текста
	//
	m_editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	m_editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	m_editor->installEventFilter(this);

	//
	// Настраиваем само представление
	//
	m_rect = m_scene->addRect(0, 0, 1, 1, QPen(), Qt::red);
	m_editorProxy = m_scene->addWidget(m_editor);
	setScene(m_scene);

	//
	// Отключаем действия полос прокрутки, чтобы в дальнейшем проксировать ими
	// полосы прокрутки самого редактора текста
	//
	horizontalScrollBar()->disconnect();
	verticalScrollBar()->disconnect();

	//
	// Синхронизация значения ролика в обе стороны
	//
	setupScrollingSynchronization(true);
}

void ScalableWrapper::paintEvent(QPaintEvent* _event)
{
	updateTextEditSize();

	//
	// Перед прорисовкой отключаем синхронизацию полос прокрутки и отматываем полосу прокрутки
	// представление наверх, для того, чтобы не смещались координаты сцены и виджет редактора
	// не уезжал за пределы видимости обёртки
	//

	setupScrollingSynchronization(false);

	int verticalValue = verticalScrollBar()->value();
	int horizontalValue = horizontalScrollBar()->value();

	verticalScrollBar()->setValue(0);
	horizontalScrollBar()->setValue(0);

	QGraphicsView::paintEvent(_event);

	verticalScrollBar()->setValue(verticalValue);
	horizontalScrollBar()->setValue(horizontalValue);

	setupScrollingSynchronization(true);
}

void ScalableWrapper::wheelEvent(QWheelEvent* _event)
{
	const int ANGLE_DIVIDER = 120;
	const qreal ZOOM_COEFFICIENT_DIVIDER = 10.;

	//
	// Собственно масштабирование
	//
	if (_event->modifiers() & Qt::ControlModifier) {
		if (_event->orientation() == Qt::Vertical) {
			//
			// zoomRange > 0 - масштаб увеличивается
			// zoomRange < 0 - масштаб уменьшается
			//
			int zoom = _event->angleDelta().y() / ANGLE_DIVIDER;
			m_zoomRange += zoom / ZOOM_COEFFICIENT_DIVIDER;
			scaleTextEdit();

			_event->accept();
		}
	}
	//
	// В противном случае эмулируем прокрутку редактора
	//
	else {
		int scrollDelta = _event->angleDelta().y() / ANGLE_DIVIDER;
		switch (_event->orientation()) {
			case Qt::Horizontal: {
				horizontalScrollBar()->setValue(
							horizontalScrollBar()->value()
							- scrollDelta * horizontalScrollBar()->singleStep());
				break;
			}

			case Qt::Vertical: {
				verticalScrollBar()->setValue(
							verticalScrollBar()->value()
							- scrollDelta * verticalScrollBar()->singleStep());
				break;
			}
		}
	}
}

bool ScalableWrapper::eventFilter(QObject* _object, QEvent* _event)
{
	bool needShowMenu = false;
	switch (_event->type()) {
		case QEvent::MouseButtonPress: {
			QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(_event);
			if (mouseEvent->button() == Qt::RightButton) {
				needShowMenu = true;
			}
			break;
		}

		case QEvent::ContextMenu: {
			needShowMenu = true;
			break;
		}

		default: {
			break;
		}
	}

	bool result = false;

	//
	// Если необходимо, то показываем контекстное меню в отдельном прокси элементе,
	// предварительно вернув ему 100% масштаб
	//
	if (needShowMenu) {
		QMenu* menu = m_editor->createStandardContextMenu();
		QGraphicsProxyWidget* menuProxy = m_editorProxy->createProxyForChildWidget(menu);

		const qreal antiZoom = 1. / m_zoomRange;
		menuProxy->setScale(antiZoom);
		menuProxy->setPos(QCursor::pos());

		menu->exec();
		delete menu;

		//
		// Событие перехвачено
		//
		result = true;
	}
	//
	// Если нет, то стандартная обработка события
	//
	else {
		result = QGraphicsView::eventFilter(_object, _event);
	}

	return result;
}

void ScalableWrapper::setupScrollingSynchronization(bool _needSync)
{
	if (_needSync) {
		connect(verticalScrollBar(), SIGNAL(valueChanged(int)),
				m_editor->verticalScrollBar(), SLOT(setValue(int)));
		connect(horizontalScrollBar(), SIGNAL(valueChanged(int)),
				m_editor->horizontalScrollBar(), SLOT(setValue(int)));
		// --
		connect(m_editor->verticalScrollBar(), SIGNAL(valueChanged(int)),
				verticalScrollBar(), SLOT(setValue(int)));
		connect(m_editor->horizontalScrollBar(), SIGNAL(valueChanged(int)),
				horizontalScrollBar(), SLOT(setValue(int)));
	} else {
		disconnect(verticalScrollBar(), SIGNAL(valueChanged(int)),
				m_editor->verticalScrollBar(), SLOT(setValue(int)));
		disconnect(horizontalScrollBar(), SIGNAL(valueChanged(int)),
				m_editor->horizontalScrollBar(), SLOT(setValue(int)));
		// --
		disconnect(m_editor->verticalScrollBar(), SIGNAL(valueChanged(int)),
				verticalScrollBar(), SLOT(setValue(int)));
		disconnect(m_editor->horizontalScrollBar(), SIGNAL(valueChanged(int)),
				horizontalScrollBar(), SLOT(setValue(int)));
	}
}

void ScalableWrapper::updateTextEditSize()
{
	//
	// Размер редактора устанавливается таким образом, чтобы скрыть масштабированные полосы
	// прокрутки (скрывать их нельзя, т.к. тогда теряются значения, которые необходимо проксировать)
	//
	const int editorWidth =
			viewport()->width() / m_zoomRange
			+ m_editor->verticalScrollBar()->width() + m_zoomRange;
	const int editorHeight =
			viewport()->height() / m_zoomRange
			+ m_editor->horizontalScrollBar()->height() + m_zoomRange;
	m_editorProxy->resize(editorWidth, editorHeight);

	//
	// Необходимые действия для корректировки значений на полосах прокрутки
	//
	const int rectWidth = m_editor->horizontalScrollBar()->maximum();
	const int rectHeight = m_editor->verticalScrollBar()->maximum();

	m_rect->setRect(0, 0, rectWidth, rectHeight);

	horizontalScrollBar()->setMaximum(rectWidth);
	verticalScrollBar()->setMaximum(rectHeight);
}

void ScalableWrapper::scaleTextEdit()
{
	const qreal MINIMUM_ZOOM_RANGE = 0.5;
	if (m_zoomRange < MINIMUM_ZOOM_RANGE) {
		m_zoomRange = MINIMUM_ZOOM_RANGE;
	}
	m_editorProxy->setScale(m_zoomRange);
}

