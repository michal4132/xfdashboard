/*
 * animation: A animation for an actor
 * 
 * Copyright 2012-2020 Stephan Haller <nomad@froevel.de>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */

/**
 * SECTION:animation
 * @short_description: An animation for an actor
 * @include: xfdashboard/animation.h
 *
 * This actor is a #XfdashboardButton and behaves exactly like a key binding which
 * performs a specified action on a specific actor when the associated key
 * combination is pressed. But instead of a key combination a button is displayed
 * and the action performed when this button is clicked.
 *
 * A #XfdashboardAnimation is usually created in the layout definition
 * of a theme but it can also be created with xfdashboard_animation_new()
 * followed by a call to xfdashboard_animation_set_target() and
 * xfdashboard_animation_set_action() to configure it.
 *
 * For example a #XfdashboardAnimation can be created which will quit the
 * application when clicked:
 *
 * |[<!-- language="C" -->
 *   ClutterActor       *actionButton;
 *
 *   actionButton=xfdashboard_animation_new();
 *   xfdashboard_animation_set_target(XFDASHBOARD_ANIMATION(actionButton), "XfdashboardApplication");
 *   xfdashboard_animation_set_action(XFDASHBOARD_ANIMATION(actionButton), "exit");
 * ]|
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libxfdashboard/animation.h>

#include <glib/gi18n-lib.h>

#include <libxfdashboard/transition-group.h>
#include <libxfdashboard/application.h>
#include <libxfdashboard/theme.h>
#include <libxfdashboard/compat.h>
#include <libxfdashboard/debug.h>


/* Define this class in GObject system */
struct _XfdashboardAnimationPrivate
{
	/* Properties related */
	gchar								*id;

	/* Instance related */
	GSList								*entries;
	gboolean							inDestruction;
};

G_DEFINE_TYPE_WITH_PRIVATE(XfdashboardAnimation,
							xfdashboard_animation,
							G_TYPE_OBJECT);

/* Properties */
enum
{
	PROP_0,

	PROP_ID,

	PROP_LAST
};

static GParamSpec* XfdashboardAnimationProperties[PROP_LAST]={ 0, };

/* Signals */
enum
{
	SIGNAL_ANIMIATION_DONE,

	SIGNAL_LAST
};

static guint XfdashboardAnimationSignals[SIGNAL_LAST]={ 0, };

/* IMPLEMENTATION: Private variables and methods */

typedef struct _XfdashboardAnimationEntry		XfdashboardAnimationEntry;
struct _XfdashboardAnimationEntry
{
	XfdashboardAnimation	*self;
	ClutterActor			*actor;
	ClutterTransition		*transition;
	guint					actorDestroyID;
	guint					transitionStoppedID;
	guint					newFrameSignalID;
};

/* Time at a transition has elapsed. This signal is only catched once for
 * a transition, so complete missing "to" values at the transition(s).
 */
static void _xfdashboard_animation_on_transition_new_frame(ClutterTransition *inTransition,
															gint inElapsed,
															gpointer inUserData)
{
	XfdashboardAnimationEntry		*entry;
	GSList							*transitions;
	GSList							*iter;

	g_return_if_fail(CLUTTER_IS_TRANSITION(inTransition));
	g_return_if_fail(inUserData);

	entry=(XfdashboardAnimationEntry*)inUserData;

	/* Check if we have to handle a transition group */
	if(XFDASHBOARD_IS_TRANSITION_GROUP(entry->transition))
	{
		/* Get list of transitions from group */
		transitions=xfdashboard_transition_group_get_transitions(XFDASHBOARD_TRANSITION_GROUP(entry->transition));

		/* Iterate through transitions of group and complete missing "to" values */
		for(iter=transitions; iter; iter=g_slist_next(iter))
		{
			ClutterPropertyTransition	*transition;
			ClutterInterval				*interval;
			GValue						*intervalToValue;
			ClutterAnimatable			*animatable;
			GValue						toValue=G_VALUE_INIT;

			/* Skip invalid transitions or transitions not modifiing properties */
			if(!iter->data) continue;

			if(!CLUTTER_IS_PROPERTY_TRANSITION(iter->data))
			{
#ifdef DEBUG
				XFDASHBOARD_DEBUG(entry->self, ANIMATION,
									"Transition %s@%p is not a ClutterPropertyTransition",
									G_OBJECT_TYPE_NAME(iter->data), iter->data);
#endif
				continue;
			}

			transition=CLUTTER_PROPERTY_TRANSITION(iter->data);

			/* Check if "to" value is missed at property-modifiing transition */
			interval=clutter_transition_get_interval(CLUTTER_TRANSITION(transition));
			if(!interval)
			{
				XFDASHBOARD_DEBUG(entry->self, ANIMATION,
									"No interval set at transition %s@%p for property %s",
									G_OBJECT_TYPE_NAME(transition), transition,
									clutter_property_transition_get_property_name(transition));
				continue;
			}

			intervalToValue=clutter_interval_peek_final_value(interval);
			if(!intervalToValue)
			{
				XFDASHBOARD_DEBUG(entry->self, ANIMATION,
									"Could not get final value from interval set at transition %s@%p for property %s",
									G_OBJECT_TYPE_NAME(transition), transition,
									clutter_property_transition_get_property_name(transition));
				continue;
			}

			if(clutter_interval_is_valid(interval))
			{
				XFDASHBOARD_DEBUG(entry->self, ANIMATION,
									"Valid interval set at transition %s@%p for property %s - no need to complete final value",
									G_OBJECT_TYPE_NAME(transition), transition,
									clutter_property_transition_get_property_name(transition));
				continue;
			}

			/* Complete missing "to" value */
			animatable=clutter_transition_get_animatable(CLUTTER_TRANSITION(transition));
			if(!animatable)
			{
				XFDASHBOARD_DEBUG(entry->self, ANIMATION,
									"Cannot determine final value from interval set at transition %s@%p for property %s as no animatable actor was set",
									G_OBJECT_TYPE_NAME(transition), transition,
									clutter_property_transition_get_property_name(transition));
				continue;
			}

			g_object_get_property(G_OBJECT(animatable),
									clutter_property_transition_get_property_name(transition),
									&toValue);
			if(G_VALUE_TYPE(&toValue)!=G_TYPE_INVALID)
			{
#ifdef DEBUG
				gchar	*valueString;

				valueString=g_strdup_value_contents(&toValue);
				XFDASHBOARD_DEBUG(entry->self, ANIMATION,
									"Set final value %s (type %s) at interval set of transition %s@%p for property %s",
									valueString, G_VALUE_TYPE_NAME(&toValue),
									G_OBJECT_TYPE_NAME(transition), transition,
									clutter_property_transition_get_property_name(transition));
				g_free(valueString);
#endif
				clutter_interval_set_final_value(interval, &toValue);
			}
			g_value_unset(&toValue);
		}

		/* Release allocated resources */
		g_slist_free_full(transitions, g_object_unref);
	}

	/* We handled the transition so remove signal handler */
	g_signal_handler_disconnect(entry->transition, entry->newFrameSignalID);
	entry->newFrameSignalID=0;
}

/* Frees an animation entry */
static void _xfdashboard_animation_entry_free(XfdashboardAnimationEntry *inData)
{
	g_return_if_fail(inData);

	/* Release allocated resources */
	if(inData->transition)
	{
		if(inData->newFrameSignalID) g_signal_handler_disconnect(inData->transition, inData->newFrameSignalID);
		if(inData->transitionStoppedID) g_signal_handler_disconnect(inData->transition, inData->transitionStoppedID);
		clutter_timeline_stop(CLUTTER_TIMELINE(inData->transition));
		g_object_unref(inData->transition);
	}
	if(inData->actor)
	{
		if(inData->actorDestroyID) g_signal_handler_disconnect(inData->actor, inData->actorDestroyID);
		clutter_actor_remove_transition(inData->actor, inData->self->priv->id);
		g_object_unref(inData->actor);
	}
	g_free(inData);
}

/* The transition, we added to an actor, has stopped. If transition reached end of
 * timeline, remove entry from. When removing entry from list of entries, we unref
 * objects and disconnect signal handlers automatically. No need to do it here.
 */
static void _xfdashboard_animation_on_transition_stopped(XfdashboardAnimation *self,
															gboolean isFinished,
															gpointer inUserData)
{
	XfdashboardAnimationPrivate		*priv;
	ClutterTransition				*destroyedTransition;
	GSList							*iter;
	XfdashboardAnimationEntry		*entry;

	g_return_if_fail(XFDASHBOARD_IS_ANIMATION(self));
	g_return_if_fail(CLUTTER_IS_TRANSITION(inUserData));

	priv=self->priv;
	destroyedTransition=CLUTTER_TRANSITION(inUserData);

	/* Only handle stopped transition if it reached end of timeline because this
	 * signal is emitted either the transition has been stopped manually or
	 * programmically or it has reached the end of timeline even after all repeats
	 * has passed. So we have to check the "is-finished" parameter.
	 */
	if(!isFinished)
	{
#ifdef DEBUG
		XFDASHBOARD_DEBUG(self, ANIMATION,
							"Do not remove entry for manually stopped transition of animation '%s'",
							priv->id);
#endif
		return;
	}

	XFDASHBOARD_DEBUG(self, ANIMATION,
						"Stopped animation '%s'",
						priv->id);

	/* Find entries to remove from list of entries */
	for(iter=priv->entries; iter; iter=g_slist_next(iter))
	{
		/* Get entry */
		entry=(XfdashboardAnimationEntry*)iter->data;
		if(!entry) continue;

		/* Check if the currently iterated entry must be freed */
		if(entry->transition==destroyedTransition)
		{
#ifdef DEBUG
			XFDASHBOARD_DEBUG(self, ANIMATION,
								"Transition %s@%p of actor %s@%p stopped, removing entry %p from animation list of animation '%s'",
								G_OBJECT_TYPE_NAME(destroyedTransition),
								destroyedTransition,
								G_OBJECT_TYPE_NAME(entry->actor),
								entry->actor,
								iter->data,
								priv->id);
#endif

			priv->entries=g_slist_remove_link(priv->entries, iter);
			_xfdashboard_animation_entry_free(entry);
			g_slist_free_1(iter);
		}
	}

	/* If list of entries is empty now, remove animation */
	if(g_slist_length(priv->entries)==0)
	{
#ifdef DEBUG
		XFDASHBOARD_DEBUG(self, ANIMATION,
							"Animation list is empty after stopped transition, unreffing animation '%s'",
							priv->id);
#endif
		g_object_unref(self);
	}
}

/* The actor, we added a transition to, is going to be destroyed, so remove entry
 * from list. When removing entry from list of entries, we unref objects and
 * disconnect signal handlers automatically. No need to do it here.
 */
static void _xfdashboard_animation_on_actor_destroyed(XfdashboardAnimation *self,
														gpointer inUserData)
{
	XfdashboardAnimationPrivate		*priv;
	ClutterActor					*destroyedActor;
	GSList							*iter;
	XfdashboardAnimationEntry		*entry;

	g_return_if_fail(XFDASHBOARD_IS_ANIMATION(self));
	g_return_if_fail(CLUTTER_IS_ACTOR(inUserData));

	priv=self->priv;
	destroyedActor=CLUTTER_ACTOR(inUserData);

	/* Find entries to remove from list of entries */
	for(iter=priv->entries; iter; iter=g_slist_next(iter))
	{
		/* Get entry */
		entry=(XfdashboardAnimationEntry*)iter->data;
		if(!entry) continue;

		/* Check if the currently iterated entry must be freed */
		if(entry->actor==destroyedActor)
		{
#ifdef DEBUG
			XFDASHBOARD_DEBUG(self, ANIMATION,
								"Actor %s@%p destroyed, removing entry %p from animation list of animation '%s'",
								G_OBJECT_TYPE_NAME(destroyedActor),
								destroyedActor,
								iter->data,
								priv->id);
#endif

			priv->entries=g_slist_remove_link(priv->entries, iter);
			_xfdashboard_animation_entry_free(entry);
			g_slist_free_1(iter);
		}
	}

	/* If list of entries is empty now, remove animation */
	if(g_slist_length(priv->entries)==0)
	{
#ifdef DEBUG
		XFDASHBOARD_DEBUG(self, ANIMATION,
							"Animation list is empty after destroyed actor, unreffing animation '%s'",
							priv->id);
#endif
		g_object_unref(self);
	}
}

/* Adds a transition group to an actor for this animation */
static void _xfdashboard_animation_real_add_animation(XfdashboardAnimation *self,
														ClutterActor *inActor,
														ClutterTransition *inTransition)
{
	XfdashboardAnimationPrivate		*priv;
	XfdashboardAnimationEntry		*data;

	g_return_if_fail(XFDASHBOARD_IS_ANIMATION(self));
	g_return_if_fail(CLUTTER_IS_ACTOR(inActor));
	g_return_if_fail(CLUTTER_IS_TRANSITION(inTransition));

	priv=self->priv;

	/* Create animation entry data */
	data=g_new0(XfdashboardAnimationEntry, 1);
	if(!data)
	{
		g_critical(_("Cannot allocate memory for animation entry with actor '%s' at animation '%s'"),
					G_OBJECT_TYPE_NAME(inActor),
					priv->id);
		return;
	}

	data->self=self;
	data->actor=g_object_ref(inActor);
	data->transition=g_object_ref(inTransition);
	data->actorDestroyID=g_signal_connect_swapped(inActor,
													"destroy",
													G_CALLBACK(_xfdashboard_animation_on_actor_destroyed),
													self);
	data->transitionStoppedID=g_signal_connect_swapped(inTransition,
														"stopped",
														G_CALLBACK(_xfdashboard_animation_on_transition_stopped),
														self);
	data->newFrameSignalID=g_signal_connect(inTransition,
											"new-frame",
											G_CALLBACK(_xfdashboard_animation_on_transition_new_frame),
											data);

	/* Add animation entry data to list to entries */
	priv->entries=g_slist_prepend(priv->entries, data);
}

/* Set animation ID */
static void _xfdashboard_animation_set_id(XfdashboardAnimation *self, const gchar *inID)
{
	XfdashboardAnimationPrivate		*priv;

	g_return_if_fail(XFDASHBOARD_IS_ANIMATION(self));
	g_return_if_fail(!inID || *inID);

	priv=self->priv;

	/* Set value if changed */
	if(g_strcmp0(priv->id, inID)!=0)
	{
		if(priv->id)
		{
			g_free(priv->id);
			priv->id=NULL;
		}

		if(inID)
		{
			priv->id=g_strdup(inID);
		}

		/* Notify about property change */
		g_object_notify_by_pspec(G_OBJECT(self), XfdashboardAnimationProperties[PROP_ID]);
	}
}

/* IMPLEMENTATION: GObject */

/* Dispose this object */
static void _xfdashboard_animation_dispose(GObject *inObject)
{
	XfdashboardAnimation			*self=XFDASHBOARD_ANIMATION(inObject);
	XfdashboardAnimationPrivate		*priv=self->priv;

	XFDASHBOARD_DEBUG(self, ANIMATION,
						"Destroying animation '%s'",
						priv->id);

	if(!priv->inDestruction)
	{
		priv->inDestruction=TRUE;

		/* Emit 'animation-done' signal*/
		g_signal_emit(self, XfdashboardAnimationSignals[SIGNAL_ANIMIATION_DONE], 0);
	}

	/* Call parent's class dispose method */
	G_OBJECT_CLASS(xfdashboard_animation_parent_class)->dispose(inObject);
}

/* Finalize this object */
static void _xfdashboard_animation_finalize(GObject *inObject)
{
	XfdashboardAnimation			*self=XFDASHBOARD_ANIMATION(inObject);
	XfdashboardAnimationPrivate		*priv=self->priv;

	XFDASHBOARD_DEBUG(self, ANIMATION,
						"Destroying animation '%s'",
						priv->id);

	g_assert(priv->inDestruction);

	/* Release our allocated variables
	 * Order is important as the ID MUST be released at last!
	 */
	if(priv->entries)
	{
		g_slist_free_full(priv->entries, (GDestroyNotify)_xfdashboard_animation_entry_free);
		priv->entries=NULL;
	}

	if(priv->id)
	{
		g_free(priv->id);
		priv->id=NULL;
	}

	/* Call parent's class dispose method */
	G_OBJECT_CLASS(xfdashboard_animation_parent_class)->finalize(inObject);
}

/* Set/get properties */
static void _xfdashboard_animation_set_property(GObject *inObject,
												guint inPropID,
												const GValue *inValue,
												GParamSpec *inSpec)
{
	XfdashboardAnimation			*self=XFDASHBOARD_ANIMATION(inObject);

	switch(inPropID)
	{
		case PROP_ID:
			_xfdashboard_animation_set_id(self, g_value_get_string(inValue));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(inObject, inPropID, inSpec);
			break;
	}
}

static void _xfdashboard_animation_get_property(GObject *inObject,
												guint inPropID,
												GValue *outValue,
												GParamSpec *inSpec)
{
	XfdashboardAnimation			*self=XFDASHBOARD_ANIMATION(inObject);
	XfdashboardAnimationPrivate		*priv=self->priv;

	switch(inPropID)
	{
		case PROP_ID:
			g_value_set_string(outValue, priv->id);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(inObject, inPropID, inSpec);
			break;
	}
}

/* Class initialization
 * Override functions in parent classes and define properties
 * and signals
 */
void xfdashboard_animation_class_init(XfdashboardAnimationClass *klass)
{
	GObjectClass			*gobjectClass=G_OBJECT_CLASS(klass);

	/* Override functions */
	klass->add_animation=_xfdashboard_animation_real_add_animation;

	gobjectClass->dispose=_xfdashboard_animation_dispose;
	gobjectClass->finalize=_xfdashboard_animation_finalize;
	gobjectClass->set_property=_xfdashboard_animation_set_property;
	gobjectClass->get_property=_xfdashboard_animation_get_property;

	/* Define signals */
	/**
	 * XfdashboardAnimation::animation-done:
	 * @self: The animation
	 *
	 * The ::animation-done signal is emitted when the animation
	 * will be destroyed, i.e. either the animation has completed
	 * or was removed while running.
	 */
	XfdashboardAnimationSignals[SIGNAL_ANIMIATION_DONE]=
		g_signal_new("animation-done",
						G_TYPE_FROM_CLASS(klass),
						G_SIGNAL_RUN_CLEANUP,
						G_STRUCT_OFFSET(XfdashboardAnimationClass, animation_done),
						NULL,
						NULL,
						g_cclosure_marshal_VOID__VOID,
						G_TYPE_NONE,
						0);

	/* Define properties */
	/**
	 * XfdashboardAnimation:id:
	 *
	 * A string with the animation ID
	 */
	XfdashboardAnimationProperties[PROP_ID]=
		g_param_spec_string("id",
								_("ID"),
								_("The animation ID"),
								NULL,
								G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(gobjectClass, PROP_LAST, XfdashboardAnimationProperties);
}

/* Object initialization
 * Create private structure and set up default values
 */
void xfdashboard_animation_init(XfdashboardAnimation *self)
{
	XfdashboardAnimationPrivate		*priv;

	priv=self->priv=xfdashboard_animation_get_instance_private(self);

	/* Set up default values */
	priv->id=NULL;
	priv->entries=NULL;
	priv->inDestruction=FALSE;
}


/* IMPLEMENTATION: Public API */

/**
 * xfdashboard_animation_new:
 * @inSender: A #ClutterActor emitting the animation signal
 * @inSignal: A string containing the signal emitted at sending actor
 *
 * Creates a new animation of type #XfdashboardAnimation matching the sending
 * actor at @inSender and the emitted signal at @inSignal.
 *
 * This function is the logical equivalent of:
 *
 * |[<!-- language="C" -->
 *   XfdashboardTheme          *theme;
 *   XfdashboardThemeAnimation *theme_animations;
 *
 *   theme=xfdashboard_application_get_theme(NULL);
 *   theme_animations=xfdashboard_theme_get_animation(theme);
 *   animation=xfdashboard_theme_animation_create(theme_animations, inSender, inSignal, NULL, NULL);
 * ]|
 *
 * Return value: (transfer full): The instance of #XfdashboardAnimation.
 */
XfdashboardAnimation* xfdashboard_animation_new(XfdashboardActor *inSender, const gchar *inSignal)
{
	XfdashboardTheme				*theme;
	XfdashboardThemeAnimation		*themeAnimation;
	XfdashboardAnimation			*animation;

	g_return_val_if_fail(XFDASHBOARD_IS_ACTOR(inSender), NULL);
	g_return_val_if_fail(inSignal && *inSignal, NULL);

	theme=xfdashboard_application_get_theme(NULL);
	themeAnimation=xfdashboard_theme_get_animation(theme);
	animation=xfdashboard_theme_animation_create(themeAnimation, inSender, inSignal, NULL, NULL);

	return(animation);
}

/**
 * xfdashboard_animation_new_with_values:
 * @inSender: A #ClutterActor emitting the animation signal
 * @inSignal: A string containing the signal emitted at sending actor
 * @inDefaultInitialValues: A %NULL-terminated list of default initial values
 * @inDefaultFinalValues: A %NULL-terminated list of default final values
 *
 * Creates a new animation of type #XfdashboardAnimation matching the sending
 * actor at @inSender and the emitted signal at @inSignal.
 *
 * A list of default values to set the initial values of the properties can be
 * provided at @inDefaultInitialValues. If it is set to %NULL then the current
 * property's value is used as initial value. This list must be %NULL-terminated.
 *
 * A list of default values to set the final values of the properties can be
 * provided at @inDefaultFinalValues. If it is set to %NULL then the current
 * property's value when the animation is started will be used as final value.
 * This list must be %NULL-terminated.
 *
 * The theme can provide an initial and final values and have higher precedence
 * as the default initial and final values passed to this function.
 *
 * The caller is responsible to free and/or unref the values in the lists
 * provided at @inDefaultInitialValues and @inDefaultFinalValues.
 *
 * This function is the logical equivalent of:
 *
 * |[<!-- language="C" -->
 *   XfdashboardTheme          *theme;
 *   XfdashboardThemeAnimation *theme_animations;
 *
 *   theme=xfdashboard_application_get_theme(NULL);
 *   theme_animations=xfdashboard_theme_get_animation(theme);
 *   animation=xfdashboard_theme_animation_create(theme_animations, inSender, inSignal, inDefaultInitialValues, inDefaultFinalValues);
 * ]|
 *
 * Return value: (transfer full): The instance of #XfdashboardAnimation.
 */
XfdashboardAnimation* xfdashboard_animation_new_with_values(XfdashboardActor *inSender,
															const gchar *inSignal,
															XfdashboardAnimationValue **inDefaultInitialValues,
															XfdashboardAnimationValue **inDefaultFinalValues)
{
	XfdashboardTheme				*theme;
	XfdashboardThemeAnimation		*themeAnimation;
	XfdashboardAnimation			*animation;

	g_return_val_if_fail(XFDASHBOARD_IS_ACTOR(inSender), NULL);
	g_return_val_if_fail(inSignal && *inSignal, NULL);

	theme=xfdashboard_application_get_theme(NULL);
	themeAnimation=xfdashboard_theme_get_animation(theme);
	animation=xfdashboard_theme_animation_create(themeAnimation, inSender, inSignal, inDefaultInitialValues, inDefaultFinalValues);

	return(animation);
}


/**
 * xfdashboard_animation_get_id:
 * @self: A #XfdashboardAnimation
 *
 * Retrieves the animation ID of @self.
 *
 * Return value: A string with animations's ID
 */
const gchar* xfdashboard_animation_get_id(XfdashboardAnimation *self)
{
	g_return_val_if_fail(XFDASHBOARD_IS_ANIMATION(self), NULL);

	return(self->priv->id);
}

/**
 * xfdashboard_animation_is_empty:
 * @self: A #XfdashboardAnimation
 *
 * Dertermines if animation at @self has any transitions.
 *
 * Return value: FALSE if animation contains transition or TRUE if empty.
 */
gboolean xfdashboard_animation_is_empty(XfdashboardAnimation *self)
{
	g_return_val_if_fail(XFDASHBOARD_IS_ANIMATION(self), TRUE);

	return(self->priv->entries ? FALSE : TRUE);
}

/**
 * xfdashboard_animation_run:
 * @self: A #XfdashboardAnimation
 *
 * Starts the animation of @self. It emits the ::done signal
 * when the animation is destroyed, either is has reached the
 * end of its timeline or was stopped before.
 */
void xfdashboard_animation_run(XfdashboardAnimation *self)
{
	XfdashboardAnimationPrivate		*priv;
	GSList							*iter;
	XfdashboardAnimationEntry		*entry;

	g_return_if_fail(XFDASHBOARD_IS_ANIMATION(self));

	priv=self->priv;

	/* Add all transition to their actors now if any available ... */
	if(priv->entries)
	{
		for(iter=priv->entries; iter; iter=g_slist_next(iter))
		{
			/* Get entry */
			entry=(XfdashboardAnimationEntry*)iter->data;
			if(!entry) continue;

			/* Add transition to actor which will start immediately */
			clutter_actor_add_transition(entry->actor, priv->id, entry->transition);
#ifdef DEBUG
			XFDASHBOARD_DEBUG(self, ANIMATION,
								"Animation '%s' added transition %p to actor %s@%p",
								priv->id,
								entry->transition,
								G_OBJECT_TYPE_NAME(entry->actor),
								entry->actor);
#endif
		}

		XFDASHBOARD_DEBUG(self, ANIMATION,
							"Started animation '%s'",
							priv->id);
	}
		/* ... otherwise destroy empty animation immediately to get callback
		 * function called and to avoid memory leaks.
		 */
		else
		{
			/* Destroy empty animation immediately */
			g_object_unref(self);
		}
}

/**
 * xfdashboard_animation_ensure_complete:
 * @self: A #XfdashboardAnimation
 *
 * Ensures that the animation at @self has reached the end
 * of its timeline but will not destroy the animation. Its
 * purpose is mainly to ensure the animation has completed
 * before it gets destroyed by other parts of the application.
 */
void xfdashboard_animation_ensure_complete(XfdashboardAnimation *self)
{
	XfdashboardAnimationPrivate		*priv;
	GSList							*iter;
	XfdashboardAnimationEntry		*entry;
	guint							duration;

	g_return_if_fail(XFDASHBOARD_IS_ANIMATION(self));

	priv=self->priv;

	for(iter=priv->entries; iter; iter=g_slist_next(iter))
	{
		/* Get entry */
		entry=(XfdashboardAnimationEntry*)iter->data;
		if(!entry) continue;

		/* Advance timeline to last frame */
		duration=clutter_timeline_get_duration(CLUTTER_TIMELINE(entry->transition));
		clutter_timeline_advance(CLUTTER_TIMELINE(entry->transition), duration);
		g_signal_emit_by_name(entry->transition, "new-frame", 0, clutter_timeline_get_elapsed_time(CLUTTER_TIMELINE(entry->transition)));
	}
}
